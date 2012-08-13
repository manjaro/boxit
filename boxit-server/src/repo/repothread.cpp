/*
 *  BoxIt - Manjaro Linux Repository Management Software
 *  Roland Singer <roland@manjaro.org>
 *
 *  Copyright (C) 2007 Free Software Foundation, Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "repothread.h"

RepoThread::RepoThread(RepoBase *repo, QObject *parent) :
    QThread(parent)
{
    this->repo = repo;
    job = JOB_NOTHING;
}



bool RepoThread::start(QString username, JOB job, QString arg1) {
    if (isRunning()) {
        return false;
    }
    else if (job == JOB_REMOVE) {
        repo->setThreadJob("remove repo");
    }
    else if (job == JOB_MOVE && !arg1.isEmpty()) {
        repo->setThreadJob("move repo");
    }
    else if (job == JOB_COPY && !arg1.isEmpty()) {
        repo->setThreadJob("copy repo");
    }
    else if (job == JOB_DB_REBUILD) {
        repo->setThreadJob("rebuild db");
    }
    else if (job == JOB_SYNC) {
        repo->setThreadJob("synchronisation");
    }
    else if (job == JOB_COMMIT) {
        repo->setThreadJob("committing");
    }
    else {
        return false;
    }

    repo->setThreadState("processing");
    this->job = job;
    this->arg1 = arg1;
    this->username = username;
    messageHistory.clear();
    QThread::start();
    return true;
}



void RepoThread::kill() {
    // Maybe it is just finishing
    usleep(250000);

    if (isRunning()) {
        terminate();
        wait();
        repo->setThreadState("killed");
    }
}



void RepoThread::run() {
    // Just wait, so the client doesn't get messages to early
    sleep(3);


    switch (job) {
        case JOB_SYNC:
        {
            repo->setTMPSync(true);

            QStringList allDBPackages, syncURLSplit = repo->getSyncURL().split("@", QString::SkipEmptyParts);
            QStringList customPackages = arg1.split(" ", QString::SkipEmptyParts);
            Sync sync(repo->getWorkPath());
            QObject::connect(&sync, SIGNAL(error(QString))  ,  this, SLOT(emitMessage(QString)));
            QObject::connect(&sync, SIGNAL(status(int,int))  ,  this, SLOT(syncStatus(int,int)));

            emitMessage(QString("%1> synchronizing files...").arg(BOXIT_FLUSH_STRING));

            if (syncURLSplit.size() < 2 || !sync.synchronize(syncURLSplit.at(1), syncURLSplit.at(0), repo->getPath() + "/" + repo->getSyncExcludeFile(), allDBPackages, QStringList() << repo->getPath(), customPackages)) {
                emitMessage("error: synchronization process failed!");
                goto errorskipupdate;
            }


            // Commit changes
            QStringList addedFiles, removedFiles;
            QString error;

            emitMessage("> removing old files...");

            // Remove all old packages which aren't in the database list if this isn't a custom sync
            if (customPackages.isEmpty()) {
                if (!removePackagesExceptList(repo->getWorkPath(), allDBPackages, removedFiles)
                        || !removePackagesExceptList(repo->getPath(), allDBPackages, removedFiles)) {
                    emitMessage("error: failed to remove old packages!");
                    goto errorskipupdate;
                }
            }

            emitMessage("> adding new files...");

            // Commit new packages to final destination
            if (!Global::commitNewPackages(repo->getWorkPath(), repo->getPath(), addedFiles, error)) {
                emitMessage(error);
                goto errorskipupdate;
            }

            // Set tmp sync to false, because there aren't any packages more in tmp
            repo->setTMPSync(false);

            // Update database
            if (!QFile::exists(repo->getPath() + "/" + repo->getRepoDB())) {
                emitMessage("> building database...");

                // Create the database
                if (!Global::buildPackageDB(repo->getPath(), repo->getRepoDB())) {
                    emitMessage("error: failed to build repository database!");
                    goto error;
                }
            }
            else {
                emitMessage("> updating database...");

                // Remove all old packages from the database
                if (!Global::removePackagesFromDatabase(repo->getPath(), repo->getRepoDB(), removedFiles)) {
                    emitMessage("error: failed to remove old packages from database!");
                    goto error;
                }

                // Add new packages to database
                if (!Global::addPackagesToDatabase(repo->getPath(), repo->getRepoDB(), addedFiles)) {
                    emitMessage("error: failed to add new packages to database");
                    goto error;
                }
            }


            // Create new repository state
            if (!repo->setNewRandomState()) {
                emitMessage("error: failed to set new repository state!");
                goto error;
            }


            // Send e-mail
            Global::sendMemoEMail(username, repo->getName(), repo->getArchitecture(), addedFiles, removedFiles);

            goto success;
        }
        case JOB_DB_REBUILD:
        {
            emitMessage("> building database...");

            if (!Global::buildPackageDB(repo->getPath(), repo->getRepoDB())) {
                emitMessage("error: failed to build repository database!");
                goto error;
            }

            goto success;
        }
        case JOB_REMOVE:
        {
            emitMessage("> removing repository...");

            QString path = QString(Global::getConfig().repoDir) + "/" + repo->getName() + "/" + repo->getArchitecture();
            if (QDir().exists(path) && !Global::rmDir(path)) {
                emitMessage(QString("error: failed to remove directory '%1'!").arg(path));
                goto error;
            }

            repo->forceLock();
            emit requestDeletion(repo);
            goto successskipupdate;
        }
        case JOB_COPY:
        case JOB_MOVE:
        {
            QString src = QString(Global::getConfig().repoDir) + "/" + repo->getName() + "/" + repo->getArchitecture();
            QString dest = QString(Global::getConfig().repoDir) + "/" + arg1;


            // Create base folder if required
            if (!QDir().exists(dest) && !QDir().mkpath(dest)) {
                emitMessage(QString("error: failed to create directory '%1'!").arg(dest));
                goto error;
            }

            dest += "/" + repo->getArchitecture();

            if (QDir().exists(dest) || !QDir().exists(src)) {
                emitMessage("error: directories are invalid!");
                goto error;
            }


            // Do the job
            if (job == JOB_MOVE) {
                emitMessage("> moving repository...");

                if (!QDir().rename(src, dest)) {
                    emitMessage(QString("error: failed to move directory '%1' to '%2'!").arg(src, dest));
                    goto error;
                }

                // Rename the db files to new repo name
                renameDB(dest, repo->getName(), arg1);

                // Remove old base repo folder if emtpy
                if (QDir(QString(Global::getConfig().repoDir) + "/" + repo->getName() ).entryList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::Name).isEmpty())
                    Global::rmDir(QString(Global::getConfig().repoDir) + "/" + repo->getName() );

                // Update new repo name
                repo->updateName(arg1);
            }
            else {
                emitMessage("> copying repository...");

                if (!Global::copyDir(src, dest, true)) {
                    emitMessage(QString("error: failed to copy directory '%1' to '%2'!").arg(src, dest));
                    goto error;
                }

                // Rename the db files to new repo name
                renameDB(dest, repo->getName(), arg1);

                emit requestNewRepo(arg1, repo->getArchitecture());
            }

            goto success;
        }
        case JOB_COMMIT:
        {
            QString errorMessage;
            QStringList addedFiles, removedFiles, removePackages = repo->getRemovePackages();


            // Remove files
            emitMessage("> removing files...");

            for (int i = 0; i < removePackages.size(); ++i) {
                QString fileName = removePackages.at(i);
                QString file = repo->getPath() + "/" + fileName;

                if (QFile::exists(file)) {
                    removedFiles.append(fileName);

                    if (!QFile::remove(file)) {
                        emitMessage(QString("error: failed to remove package '%1'!").arg(file));
                        goto error;
                    }
                }

                if (QFile::exists(file + BOXIT_SIGNATURE_ENDING) && !QFile::remove(file + BOXIT_SIGNATURE_ENDING)) {
                    emitMessage(QString("error: failed to remove signature '%1'!").arg(file + BOXIT_SIGNATURE_ENDING));
                    goto error;
                }
            }


            // Move new files
            emitMessage("> copying files...");

            if (!Global::commitNewPackages(repo->getWorkPath(), repo->getPath(), addedFiles, errorMessage)) {
                emitMessage(errorMessage);
                goto error;
            }


            // Update database
            if (!QFile::exists(repo->getPath() + "/" + repo->getRepoDB())) {
                emitMessage("> creating package database...");

                // Create Database
                if (!Global::buildPackageDB(repo->getPath(), repo->getRepoDB())) {
                    emitMessage("error: failed to create package database!");
                    goto error;
                }
            }
            else {
                emitMessage("> updating package database...");

                // Remove packages from database
                if (!Global::removePackagesFromDatabase(repo->getPath(), repo->getRepoDB(), removedFiles)) {
                    emitMessage("error: failed to remove packages from database!");
                    goto error;
                }

                // Add new packages to database
                if (!Global::addPackagesToDatabase(repo->getPath(), repo->getRepoDB(), addedFiles)) {
                    emitMessage("error: failed to add new packages to database!");
                    goto error;
                }
            }


            // Create new repository state
            if (!repo->setNewRandomState()) {
                emitMessage("error: failed to set new repository state!");
                goto error;
            }

            // Send e-mail
            Global::sendMemoEMail(username, repo->getName(), repo->getArchitecture(), addedFiles, removedFiles);

            goto success;
        }
        default:
            goto error;
    }


success:
    emitMessage("> updating internal data...");
    if (!repo->update()) {
        emitMessage("error: failed to update internal data!");
        goto errorskipupdate;
    }

successskipupdate:
    repo->setThreadState("success");
    emit finished(repo->getName(), repo->getArchitecture(), true);
    return;

error:
    repo->update(); // Error isn't important here

errorskipupdate:
    repo->setThreadState("failed");
    emit finished(repo->getName(), repo->getArchitecture(), false);
}



QString RepoThread::getMessageHistory() {
    return messageHistory.join("\n");
}




//###
//### Private methods
//###



void RepoThread::renameDB(const QString path, const QString oldName, const QString newName) {
    if (QFile::exists(path + "/" + oldName + BOXIT_DB_ENDING))
        QFile::rename(path + "/" + oldName + BOXIT_DB_ENDING, path + "/" + newName + BOXIT_DB_ENDING);

    if (QFile::exists(path + "/" + oldName + BOXIT_DB_LINK_ENDING))
        QFile::rename(path + "/" + oldName + BOXIT_DB_LINK_ENDING, path + "/" + newName + BOXIT_DB_LINK_ENDING);
}



bool RepoThread::removePackagesExceptList(const QString path, const QStringList &list, QStringList &removedFiles) {
    QStringList files = QDir(path).entryList(QString(BOXIT_FILE_FILTERS).split(" ", QString::SkipEmptyParts), QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    removedFiles.clear();

    for (int i = 0; i < files.size(); ++i) {
        QString file = files.at(i);

        if (list.contains(file))
            continue;

        removedFiles.append(file);
        file = path + "/" + file;

        if (!QFile::remove(file)
                || (QFile::exists(file + BOXIT_SIGNATURE_ENDING)
                    && !QFile::remove(file + BOXIT_SIGNATURE_ENDING)))
            return false;
    }

    return true;
}



void RepoThread::addToHistory(QString str) {
    if (messageHistory.size() > 40) {
        messageHistory.removeFirst();
        messageHistory.first() = "...";
    }

    if (messageHistory.isEmpty()) {
        messageHistory.append(str);
        return;
    }

    bool strF = str.startsWith(BOXIT_FLUSH_STRING);
    bool mesF = messageHistory.last().startsWith(BOXIT_FLUSH_STRING);

    if (strF && mesF) {
        messageHistory.last() = str;
    }
    else if (!strF && mesF) {
        messageHistory.last().remove(0, QString(BOXIT_FLUSH_STRING).length());
        messageHistory.append(str);
    }
    else {
        messageHistory.append(str);
    }
}



//###
//### Private slots
//###



void RepoThread::emitMessage(QString msg) {
    // Add to history
    addToHistory(msg);

    emit message(repo->getName(), repo->getArchitecture(), msg);
}



void RepoThread::syncStatus(int index, int total) {
    QString msg = QString("%1> synchronizing files... [%2/%3]").arg(BOXIT_FLUSH_STRING, QString::number(index), QString::number(total));

    // Add to history
    addToHistory(msg);

    emit message(repo->getName(), repo->getArchitecture(), msg);
}
