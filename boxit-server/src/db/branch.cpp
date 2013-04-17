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

#include "branch.h"

Branch::Branch(const QString name, const QString path) :
    name(name),
    path(path)
{
    moveToThread(qApp->thread());
    setParent(qApp);
}



Branch::~Branch() {
    qDeleteAll(repos);
    repos.clear();
}



bool Branch::init() {
    // Read config
    if (!readConfig())
        return false;


    // Read exclude file
    if (!readExcludeContentConfig())
        return false;


    // Get branch repos
    qDeleteAll(repos);
    repos.clear();

    QStringList repoList = QDir(path).entryList(QDir::AllDirs | QDir::NoDotAndDotDot, QDir::Name);
    QStringList architectures = QString(BOXIT_ARCHITECTURES).split(" ", QString::SkipEmptyParts);

    foreach (QString repoName, repoList) {
        foreach (QString architecture, architectures) {
            QString repoPath = path + "/" + repoName + "/" + architecture;

            if (!QDir(repoPath).exists())
                continue;

            Repo *repo = new Repo(name, repoName, architecture, repoPath);

            if (!repo->init()) {
                cerr << "warning: failed to init repo '" << repoPath.toAscii().data() << "'!" << endl;
                delete (repo);
                continue;
            }

            connect(repo, SIGNAL(requestNewBranchState())   ,   this, SLOT(setNewBranchState()));
            connect(repo, SIGNAL(threadFailed(Repo*,int))   ,   this, SLOT(repoThreadFailed(Repo*,int)));
            connect(repo, SIGNAL(threadWaiting(Repo*,int))  ,   this, SLOT(repoThreadWaiting(Repo*,int)));
            connect(repo, SIGNAL(threadFinished(Repo*,int))  ,   this, SLOT(repoThreadFinished(Repo*,int)));

            repos.append(repo);
        }
    }


    return true;
}



bool Branch::setExcludeFilesContent(const QString content) {
    // Save config
    QFile file(path + "/" + BOXIT_DB_SYNC_EXCLUDE);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << content;
    file.close();

    return readExcludeContentConfig();
}



bool Branch::setUrl(const QString url) {
    this->url = url;

    return updateConfig();
}



//###
//### Private
//###


bool Branch::readExcludeContentConfig() {
    // Clean up first
    excludeFiles.clear();
    excludeFilesContent.clear();

    // Read exclude file
    QFile file(path + "/" + BOXIT_DB_SYNC_EXCLUDE);
    if (!file.exists())
        return true;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    excludeFilesContent = QString(in.readAll());
    file.close();

    // Fill list
    QStringList list = excludeFilesContent.split("\n", QString::KeepEmptyParts);

    foreach (QString line, list) {
        line = line.split("#", QString::KeepEmptyParts).first().trimmed();
        if (line.isEmpty())
            continue;

        excludeFiles.append(line);
    }

    excludeFiles.removeDuplicates();

    return true;
}



bool Branch::readConfig() {
    // Read config
    QFile file(path + "/" + BOXIT_DB_CONFIG);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().split("#", QString::KeepEmptyParts).first().trimmed();
        if (line.isEmpty() || !line.contains("="))
            continue;

        QString arg1 = line.split("=").first().toLower().trimmed();
        QString arg2 = line.split("=").last().trimmed();

        if (arg1 == "syncurl") {
            url = arg2;
        }
    }
    file.close();

    return true;
}



bool Branch::updateConfig() {
    QFile file(path + "/" + BOXIT_DB_CONFIG);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "###\n### BoxIt Branch Config\n###\n";
    out << "\nsyncurl=" << url;

    file.close();
    return true;
}



void Branch::setNewBranchState() {
    QMutexLocker locker(&setBranchStateMutex);

    QFile file(path + "/" + BOXIT_BRANCH_STATE_FILE);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        cerr << "error: failed to save '" << QString(path + "/" + BOXIT_BRANCH_STATE_FILE).toUtf8().data() << "'!" << endl;
        return;
    }

    QTextStream out(&file);
    out << "###\n### BoxIt branch state file\n###\n";
    out << "\n# Unique hash code representing current branch state.\n# This hash code changes as soon as anything changes in this branch.";
    out << "\nstate=" << QString(QCryptographicHash::hash(QString(QDateTime::currentDateTime().toString(Qt::ISODate) + QString::number(qrand())).toLocal8Bit(), QCryptographicHash::Sha1).toHex());
    out << "\n\n# Date and time of the last branch change.";
    out << "\ndate=" << QDateTime::currentDateTime().toString(Qt::ISODate);

    file.close();

    // Fix file permission
    Global::fixFilePermission(path + "/" + BOXIT_BRANCH_STATE_FILE);
}



void Branch::repoThreadFailed(Repo*, int threadSessionID) {
    QMutexLocker locker(&repoThreadMutex);

    // Abort all other threads with the same session ID
    for (int i = 0; i < repos.size(); ++i) {
        Repo *r = repos[i];

        if (r->getThreadSessionID() == threadSessionID && r->isRunning())
            r->abort();
    }

    // Inform the socket with the same session ID
    Status::branchSessionChanged(threadSessionID, false);
}



void Branch::repoThreadWaiting(Repo *repo, int threadSessionID) {
    QMutexLocker locker(&repoThreadMutex);

    // Check if another repo thread with the same session ID is running
    bool found = false;

    for (int i = 0; i < repos.size(); ++i) {
        Repo *r = repos[i];

        if (r != repo
                && r->isRunning()
                && !r->waitingForCommit()
                && r->getThreadSessionID() == threadSessionID) {
            found = true;
            break;
        }
    }

    // Next Thread will trigger the commit
    if (found)
        return;


    // Commit all changes
    for (int i = 0; i < repos.size(); ++i) {
        Repo *r = repos[i];

        if (r->getThreadSessionID() == threadSessionID && r->waitingForCommit())
            r->commit();
    }
}



void Branch::repoThreadFinished(Repo *repo, int threadSessionID) {
    QMutexLocker locker(&repoThreadMutex);

    // Check if another repo thread with the same session ID is running
    bool found = false;

    for (int i = 0; i < repos.size(); ++i) {
        Repo *r = repos[i];

        if (r != repo
                && r->isRunning()
                && r->getThreadSessionID() == threadSessionID) {
            found = true;
            break;
        }
    }

    // Next Thread will trigger the signal
    if (found)
        return;

    // Inform the socket with the same session ID
    Status::branchSessionChanged(threadSessionID, true);
}
