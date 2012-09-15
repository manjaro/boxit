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

#include "pool.h"


//###
//### Private static variables
//###

Pool Pool::self;
bool Pool::locked = false;
Repo* Pool::lockedRepo = NULL;
QString Pool::lockedUsername = "-";
QStringList Pool::addFiles, Pool::removeFiles;



//###
//### Public static methods
//###


bool Pool::isLocked() {
    return (locked || self.isRunning());
}



bool Pool::isLocked(Repo *repo) {
    return (isLocked() && lockedRepo == repo);
}



bool Pool::lock(Repo *repo, QString username) {
    if (!lock())
        return false;

    lockedRepo = repo;
    lockedUsername = username;
    return true;
}



bool Pool::lock() {
    if (isLocked())
        return false;

    locked = true;
    addFiles.clear();
    removeFiles.clear();
    return true;
}



void Pool::unlock() {
    locked = false;
}



void Pool::addFile(QString file) {
    addFiles.append(file);
}



void Pool::removeFile(QString file) {
    removeFiles.append(file);
}



void Pool::killThread() {
    self.kill();
}


QString Pool::getMessageHistory() {
    return self.messageHistory.join("\n");
}


bool Pool::commit() {
    if (self.isRunning())
        return false;

    self.job = JOB_COMMIT;
    self.jobString = "committing";
    self.arg1.clear();
    self.start();
    return true;
}



bool Pool::synchronize(QString &customPackages) {
    if (self.isRunning())
        return false;

    self.job = JOB_SYNC;
    self.jobString = "synchronizing";
    self.arg1 = customPackages;
    self.start();
    return true;
}



//###
//### Public methods
//###



Pool::Pool() {
    processSuccess = true;
    stateString = "-";
    jobString = "-";
}




void Pool::kill() {
    // Maybe it is just finishing
    usleep(250000);

    if (isRunning()) {
        terminate();
        wait();
        stateString = "killed";
        processSuccess = false;
        unlock();
        emit finished(false);
    }
}



void Pool::run() {
    stateString = "processing";
    messageHistory.clear();
    addFiles.removeDuplicates();
    removeFiles.removeDuplicates();

    // Just wait, so the client doesn't get messages to early
    sleep(2);

    switch (job) {
    case JOB_SYNC:
    {
        QStringList allDBPackages, syncURLSplit = lockedRepo->getSyncURL().split("@", QString::SkipEmptyParts);
        QStringList customPackages = arg1.split(" ", QString::SkipEmptyParts);

        Sync sync(Global::getConfig().poolDir);
        QObject::connect(&sync, SIGNAL(error(QString))  ,  this, SLOT(emitMessage(QString)));
        QObject::connect(&sync, SIGNAL(status(int,int))  ,  this, SLOT(syncStatus(int,int)));

        emitMessage(QString("%1> synchronizing files...").arg(BOXIT_FLUSH_STRING));

        addFiles.clear();
        removeFiles.clear();

        if (syncURLSplit.size() < 2 || !sync.synchronize(syncURLSplit.at(1), syncURLSplit.at(0), lockedRepo->getPath() + "/" + lockedRepo->getSyncExcludeFile(), allDBPackages, addFiles, QStringList(), customPackages)) {
            emitMessage("error: synchronization process failed!");
            goto errorskipupdate;
        }


        // Determind all old packages which aren't in the database list if this isn't a custom sync
        if (customPackages.isEmpty()) {
            emitMessage("> determinding old files...");

            QStringList files = QDir(lockedRepo->getPath()).entryList(QString(BOXIT_FILE_FILTERS).split(" ", QString::SkipEmptyParts), QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

            for (int i = 0; i < files.size(); ++i) {
                QString file = files.at(i);

                if (!allDBPackages.contains(file))
                    removeFiles.append(file);
            }
        }
    }
    case JOB_COMMIT:
    {
        // Remove files
        emitMessage("> removing symlinks...");

        for (int i = 0; i < removeFiles.size(); ++i) {
            QString filename = removeFiles.at(i);
            QString file = lockedRepo->getPath() + "/" + filename;

            if (QFile::exists(file) && !QFile::remove(file)) {
                emitMessage(QString("error: failed to remove symlink '%1'!").arg(file));
                goto error;
            }

            if (QFile::exists(file + BOXIT_SIGNATURE_ENDING) && !QFile::remove(file + BOXIT_SIGNATURE_ENDING)) {
                emitMessage(QString("error: failed to remove signature symlink '%1'!").arg(file + BOXIT_SIGNATURE_ENDING));
                goto error;
            }
        }


        // Symlink new files
        emitMessage("> linking new files...");

        for (int i = 0; i < addFiles.size(); ++i) {
            QString filename = addFiles.at(i);
            QString file = Global::getConfig().poolDir + "/" + filename;
            QString dest = lockedRepo->getPath() + "/" + filename;

            if (!QFile::exists(file) || (!QFile::exists(dest) && !QFile::link(file, dest))) {
                emitMessage(QString("error: failed to link '%1'!").arg(file));
                goto error;
            }

            if (QFile::exists(file + BOXIT_SIGNATURE_ENDING) && !QFile::exists(dest + BOXIT_SIGNATURE_ENDING) && !QFile::link(file + BOXIT_SIGNATURE_ENDING, dest + BOXIT_SIGNATURE_ENDING))  {
                emitMessage(QString("error: failed to link '%1'!").arg(file + BOXIT_SIGNATURE_ENDING));
                goto error;
            }
        }



        emitMessage("> updating internal data...");
        if (!lockedRepo->update()) {
            emitMessage("error: failed to update internal data!");
            goto errorskipupdate;
        }


        // Update database
        emitMessage("> updating package database...");
        if (!updatePackageDatabase()) {
            emitMessage("error: failed to update package database!");
            goto error;
        }


        // Create new repository state
        if (!lockedRepo->setNewRandomState()) {
            emitMessage("error: failed to set new repository state!");
            goto error;
        }

        // Send e-mail
        Global::sendMemoEMail(lockedUsername, lockedRepo->getName(), lockedRepo->getArchitecture(), addFiles, removeFiles);

        goto successskipupdate;
    }
    default:
        goto error;
    }


success:
    emitMessage("> updating internal data...");
    if (!lockedRepo->update()) {
        emitMessage("error: failed to update internal data!");
        goto errorskipupdate;
    }

successskipupdate:
    stateString = "success";
    emit finished(true);
    processSuccess = true;
    unlock();
    return;

error:
    lockedRepo->update(); // Error isn't important here

errorskipupdate:
    stateString = "failed";
    emit finished(false);
    processSuccess = false;
    unlock();
}




//###
//### Private methods
//###



void Pool::addToHistory(QString str) {
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



bool Pool::updatePackageDatabase() {
    // Create a folder to unpack the database to
    QString dbPath = QString(BOXIT_TMP) + "/" + QString::number(qrand());
    QString dbFile = lockedRepo->getPath() + "/" + lockedRepo->getRepoDB();
    QStringList dbPackages, packagesToRemove, packagesToAdd;
    QStringList packages = lockedRepo->getPackages();

    if (QFile::exists(dbFile)) {
        // Create work folder
        if ((QDir(dbPath).exists() && !Global::rmDir(dbPath))
                || !QDir().mkpath(dbPath))
            return false;

        // Unpack the database file
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.setWorkingDirectory(dbPath);
        process.start("tar", QStringList() << "-xf" << dbFile << "-C" << dbPath);

        if (!process.waitForFinished(60000)) {
            emitMessage("error: archive extract process timeout!");
            return false;
        }

        if (process.exitCode() != 0) {
            emitMessage("error: archive extract process failed: " + QString(process.readAll()));
            return false;
        }


        // Get packages list
        dbPackages = QDir(dbPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

        // Cleanup
        Global::rmDir(dbPath); // Error isn't important...


        // Get packages to remove
        for (int i = 0; i < dbPackages.size(); ++i) {
            QString dbPackage = dbPackages.at(i) + "-"; // Because arch is missing -> getNameofPKG fix
            QString name = Global::getNameofPKG(dbPackage);
            QString version = Global::getVersionofPKG(dbPackage);
            bool found = false;

            for (int i = 0; i < packages.size(); ++i) {
                QString package = packages.at(i);

                if (name == Global::getNameofPKG(package) && version == Global::getVersionofPKG(package)) {
                    found = true;
                    break;
                }
            }

            if (!found)
                packagesToRemove.append(name);
        }
    }

    // Get package to add
    for (int i = 0; i < packages.size(); ++i) {
        QString package = packages.at(i);
        QString name = Global::getNameofPKG(package);
        QString version = Global::getVersionofPKG(package);
        bool found = false;

        for (int i = 0; i < dbPackages.size(); ++i) {
            QString dbPackage = dbPackages.at(i) + "-"; // Because arch is missing -> getNameofPKG fix

            if (name == Global::getNameofPKG(dbPackage) && version == Global::getVersionofPKG(dbPackage)) {
                found = true;
                break;
            }
        }

        if (!found)
            packagesToAdd.append(package);
    }


    // Remove old packages from database
    if (!removePackagesFromDatabase(packagesToRemove))
        return false;

    // Add new packages to database
    if (!addPackagesToDatabase(packagesToAdd))
        return false;

    return true;
}



bool Pool::addPackagesToDatabase(const QStringList &packages) {
    if (packages.isEmpty())
        return true;

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setWorkingDirectory(lockedRepo->getPath());
    process.start("repo-add", QStringList() << lockedRepo->getRepoDB() <<  packages);

    if (!process.waitForFinished(-1))
        return false;

    if (process.exitCode() != 0)
        return false;

    return true;
}



bool Pool::removePackagesFromDatabase(const QStringList &packages) {
    if (packages.isEmpty())
        return true;

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setWorkingDirectory(lockedRepo->getPath());
    process.start("repo-remove", QStringList() << lockedRepo->getRepoDB() <<  packages);

    if (!process.waitForFinished(-1))
        return false;

    if (process.exitCode() != 0)
        return false;

    return true;
}



//###
//### Private slots
//###



void Pool::emitMessage(QString msg) {
    // Add to history
    addToHistory(msg);

    emit message(msg);
}



void Pool::syncStatus(int index, int total) {
    QString msg = QString("%1> synchronizing files... [%2/%3]").arg(BOXIT_FLUSH_STRING, QString::number(index), QString::number(total));

    // Add to history
    addToHistory(msg);

    emit message(msg);
}
