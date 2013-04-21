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

#include "database.h"

QMutex Database::mutex;
Sync Database::sync;
QList<Branch*> Database::branches;
QMap<int, Database::PoolLock> Database::lockedPoolFiles;
QStringList Database::poolFiles;



void Database::init() {
    QMutexLocker locker(&mutex);

    // Create pool directories if they don't exist
    if (!QDir(Global::getConfig().overlayPoolDir).exists() && !QDir().mkpath(Global::getConfig().overlayPoolDir))
        cerr << "warning: failed to create overlay pool directory'!" << endl;

    if (!QDir(Global::getConfig().syncPoolDir).exists() && !QDir().mkpath(Global::getConfig().syncPoolDir))
        cerr << "warning: failed to create overlay pool directory'!" << endl;


    qDeleteAll(branches);
    branches.clear();

    QString dbDir = Global::getConfig().repoDir;
    QStringList branchList = QDir(dbDir).entryList(QDir::AllDirs | QDir::NoDotAndDotDot, QDir::Name);

    foreach (QString branchName, branchList) {
        if (!QFile::exists(dbDir + "/" + branchName + "/" + BOXIT_DB_CONFIG))
            continue; // Seams to be no branch folder...

        Branch *branch = new Branch(branchName, dbDir + "/" + branchName);

        if (!branch->init()) {
            cerr << "warning: failed to init branch '" << branchName.toUtf8().data() << "'!" << endl;
            delete (branch);
            continue;
        }

        branches.append(branch);
    }


    // Get all pool files
    poolFiles = QDir(Global::getConfig().overlayPoolDir).entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
}



QStringList Database::getBranches() {
    QMutexLocker locker(&mutex);

    QStringList list;

    for (int i = 0; i < branches.size(); ++i) {
        list.append(branches.at(i)->name);
    }

    list.sort();

    return list;
}



bool Database::getBranchUrl(const QString branchName, QString & url) {
    QMutexLocker locker(&mutex);

    // Get branch
    Branch *branch = _getBranch(branchName);
    if (branch == NULL)
        return false;

    url = branch->getUrl();

    return true;
}



bool Database::setBranchUrl(const QString branchName, const QString url) {
    QMutexLocker locker(&mutex);

    // Get branch
    Branch *branch = _getBranch(branchName);
    if (branch == NULL)
        return false;

    return branch->setUrl(url);
}



bool Database::getBranchSyncExcludeFiles(const QString branchName, QString & excludeFilesContent) {
    QMutexLocker locker(&mutex);

    // Get branch
    Branch *branch = _getBranch(branchName);
    if (branch == NULL)
        return false;

    excludeFilesContent = branch->getExcludeFilesContent();

    return true;
}



bool Database::setBranchSyncExcludeFiles(const QString branchName, const QString excludeFilesContent) {
    QMutexLocker locker(&mutex);

    // Get branch
    Branch *branch = _getBranch(branchName);
    if (branch == NULL)
        return false;

    return branch->setExcludeFilesContent(excludeFilesContent);
}



bool Database::isBranchLocked(const QString branchName) {
    QMutexLocker locker(&mutex);

    // Get branch
    Branch *branch = _getBranch(branchName);
    if (branch == NULL)
        return false;

    for (int i = 0; i < branch->repos.size(); ++i) {
        if (branch->repos[i]->isLocked())
            return true;
    }

    return false;
}



bool Database::getRepos(const QString branchName, QList<Database::RepoInfo> & list) {
    QMutexLocker locker(&mutex);

    list.clear();

    // Get branch
    Branch *branch = _getBranch(branchName);
    if (branch == NULL)
        return false;

    // Add repos to list
    for (int i = 0; i < branch->repos.size(); ++i) {
        Repo *repo = branch->repos[i];

        Database::RepoInfo info;
        info.name = repo->getName();
        info.architecture = repo->getArchitecture();
        info.state = repo->getState();
        info.isSyncRepo = repo->isSyncable();
        info.overlayPackages = repo->getOverlayPackages();
        info.syncPackages = repo->getSyncPackages();

        list.append(info);
    }

    return true;
}



bool Database::lockRepo(const QString branchName, const QString repoName, const QString repoArchitecture, const int sessionID, const QString username) {
    QMutexLocker locker(&mutex);

    Repo *repo = _getRepo(branchName, repoName, repoArchitecture);
    if (repo == NULL)
        return false;

    return repo->lock(sessionID, username);
}



bool Database::adjustRepoFiles(const QString branchName, const QString repoName, const QString repoArchitecture, const int sessionID, const QStringList & addPackages, const QStringList & removePackages) {
    QMutexLocker locker(&mutex);

    Repo *repo = _getRepo(branchName, repoName, repoArchitecture);
    if (repo == NULL)
        return false;

    // Check if repo is locked by session ID
    if (repo->getLockedSessionID() != sessionID)
        return false;

    // Check if the new packages exist in the overlay pool
    foreach (QString package, addPackages) {
        if (!poolFiles.contains(package))
            return false;
    }

    return repo->adjustPackages(addPackages, removePackages);
}



void Database::releaseRepoLock(const int sessionID) {
    QMutexLocker locker(&mutex);

    // Release all locked repos
    _releaseRepoLock(sessionID);
}



bool Database::lockPoolFiles(const int sessionID, const QString username, const QStringList & files) {
    QMutexLocker locker(&mutex);

    // Check if files are already locked
    QMap<int, PoolLock>::const_iterator i = lockedPoolFiles.constBegin();
    while (i != lockedPoolFiles.constEnd()) {
        foreach (QString file, i.value().files) {
            if (files.contains(file))
                return false;
        }

        ++i;
    }

    // Lock new files
    PoolLock lock;
    lock.username = username;
    lock.files = files;

    lockedPoolFiles[sessionID] = lock;

    return true;
}



bool Database::checkPoolFilesExists(const QStringList & files, QStringList & missingFiles) {
    QMutexLocker locker(&mutex);

    bool success = true;
    missingFiles.clear();

    // Check if files exists...
    foreach (QString file, files) {
        if (!poolFiles.contains(file)) {
            missingFiles.append(file);
            success = false;
        }
    }

    missingFiles.removeDuplicates();

    return success;
}



bool Database::getPoolFileCheckSum(const QString file, QByteArray & checkSum) {
    QMutexLocker locker(&mutex);

    if (!poolFiles.contains(file))
        return false;

    checkSum = Global::sha1CheckSum(Global::getConfig().overlayPoolDir + "/" + file);

    return true;
}



bool Database::moveFilesToPool(const int sessionID, const QString path, QStringList files) {
    QMutexLocker locker(&mutex);

    files.removeDuplicates();

    // Check if pool lock exists
    if (!lockedPoolFiles.contains(sessionID))
        return false;

    // Check if files already exists in the pool directory
    foreach (QString file, poolFiles) {
        if (files.contains(file))
            return false;
    }

    // Check if files are really locked by the sessionID
    PoolLock lock = lockedPoolFiles.value(sessionID);

    foreach (QString file, files) {
        if (!lock.files.contains(file))
            return false;
    }

    // Move files to pool directory
    const QString poolDir = Global::getConfig().overlayPoolDir;
    QDir dir;
    bool success = true;

    foreach (QString file, files) {
        if (dir.rename(path + "/" + file, poolDir + "/" + file)) {
            poolFiles.append(file);

            // Fix file permission
            Global::fixFilePermission(poolDir + "/" + file);
        }
        else {
            qDebug(file.toUtf8());
            success = false;
        }
    }

    return success;
}



void Database::releasePoolLock(const int sessionID) {
    QMutexLocker locker(&mutex);

    // Release locked pool session
    _releasePoolLock(sessionID);
}



bool Database::synchronizeBranch(const QString branchName, const QString username, int & syncSessionID) {
    QMutexLocker locker(&mutex);

    if (sync.isRunning())
        return false;

    // Get branch
    Branch *branch = _getBranch(branchName);
    if (branch == NULL)
        return false;

    syncSessionID = sync.start(username, branch);

    return (syncSessionID >= 0);
}



bool Database::snapshotBranch(const QString sourceBranchName, const QString destBranchName, const QString username, const int sessionID) {
    QMutexLocker locker(&mutex);

    bool returnCode = false;

    if (sourceBranchName == destBranchName)
        return false;

    // Get branches
    Branch *sourceBranch = _getBranch(sourceBranchName);
    Branch *destBranch = _getBranch(destBranchName);
    if (sourceBranch == NULL || destBranch == NULL)
        return false;


    // Check if any repository of source or destination branch is locked
    for (int i = 0; i < sourceBranch->repos.size(); ++i) {
        if (sourceBranch->repos[i]->isLocked())
            return false;
    }

    for (int i = 0; i < destBranch->repos.size(); ++i) {
        if (destBranch->repos[i]->isLocked())
            return false;
    }


    // Try to lock branches
    for (int i = 0; i < sourceBranch->repos.size(); ++i) {
        if (!sourceBranch->repos[i]->lock(sessionID, username))
            goto unlockRepos;
    }

    for (int i = 0; i < destBranch->repos.size(); ++i) {
        if (!destBranch->repos[i]->lock(sessionID, username))
            goto unlockRepos;
    }


    // Remove destination branch
    if (!Global::rmDir(destBranch->path))
        goto unlockRepos;

    // Copy branch
    if (!Global::copyDir(sourceBranch->path, destBranch->path, true))
        goto unlockRepos;

    // Remove old branch from list
    for (int i = 0; i < branches.size(); ++i) {
        Branch *branch = branches[i];

        if (branch->name != destBranch->name)
            continue;

        branches.removeAt(i);
        delete branch;
        break;
    }

    // Create new branch and add it to the list
    destBranch = new Branch(destBranchName, Global::getConfig().repoDir + "/" + destBranchName);

    if (!destBranch->init()) {
        cerr << "error: failed to init branch '" << destBranchName.toUtf8().data() << "'!" << endl;
        delete (destBranch);
        goto unlockSourceRepos;
    }

    branches.append(destBranch);

    // Send e-mail
    Global::sendMemoEMail(QString("### BoxIt memo ###\n\nUser %1 created a snapshot of branch '%2' to '%3'.").arg(username, sourceBranchName, destBranchName), QStringList());


    returnCode = true;

unlockRepos:

    // Unlock repos again
    for (int i = 0; i < destBranch->repos.size(); ++i) {
        Repo *repo = destBranch->repos[i];

        if (repo->getLockedSessionID() == sessionID)
            repo->unlock();
    }

unlockSourceRepos:
    for (int i = 0; i < sourceBranch->repos.size(); ++i) {
        Repo *repo = sourceBranch->repos[i];

        if (repo->getLockedSessionID() == sessionID)
            repo->unlock();
    }

    return returnCode;
}



void Database::releaseSession(const int sessionID) {
    QMutexLocker locker(&mutex);

    // Release locked pool session
    _releasePoolLock(sessionID);

    // Release all locked repos
    _releaseRepoLock(sessionID);
}



void Database::removeOrphanPoolFiles() {
    // Lock mutex
    mutex.lock();

    // Check if sync is running
    if (sync.isRunning()) {
        mutex.unlock();
        return;
    }

    // Check if any repository is locked
    for (int i = 0; i < branches.size(); ++i) {
        Branch *branch = branches[i];

        for (int i = 0; i < branch->repos.size(); ++i) {
            if (branch->repos[i]->isLocked()) {
                mutex.unlock();
                return;
            }
        }
    }

    // Try to lock every repository
    for (int i = 0; i < branches.size(); ++i) {
        Branch *branch = branches[i];

        for (int i = 0; i < branch->repos.size(); ++i) {
            if (!branch->repos[i]->lock(BOXIT_SYSTEM_SESSION_ID, BOXIT_SYSTEM_USERNAME)) {
                // Remove all locks
                for (int i = 0; i < branches.size(); ++i) {
                    Branch *branch = branches[i];

                    for (int i = 0; i < branch->repos.size(); ++i) {
                        Repo *repo = branch->repos[i];

                        if (repo->getLockedSessionID() == BOXIT_SYSTEM_SESSION_ID)
                            repo->unlock();
                    }
                }

                mutex.unlock();
                return;
            }
        }
    }

    // Get all pool files
    const QString overlayPoolPath = Global::getConfig().overlayPoolDir;
    const QString syncPoolPath = Global::getConfig().syncPoolDir;
    QStringList syncPoolFiles = QDir(syncPoolPath).entryList(QDir::Files | QDir::NoDotAndDotDot);
    QStringList overlayPoolFiles = QDir(overlayPoolPath).entryList(QDir::Files | QDir::NoDotAndDotDot);

    // Unlock mutex
    mutex.unlock();



    // Get all packages
    QStringList allOverlayPackages, allSyncPackages;

    for (int i = 0; i < branches.size(); ++i) {
        Branch *branch = branches[i];

        for (int i = 0; i < branch->repos.size(); ++i) {
            Repo *repo = branch->repos[i];

            allOverlayPackages.append(repo->getOverlayPackages());
            allSyncPackages.append(repo->getSyncPackages());
        }
    }

    // Get all orphan files
    _keepOrphanFiles(overlayPoolFiles, allOverlayPackages);
    _keepOrphanFiles(syncPoolFiles, allSyncPackages);

    QDateTime currentDateTime = QDateTime::currentDateTime();

    // Remove all old overlay files
    foreach (const QString file, overlayPoolFiles) {
        // Check when it was last modified
        QString filePath = overlayPoolPath + "/" + file;
        QFileInfo info(filePath);

        if (info.lastModified().daysTo(currentDateTime) >= BOXIT_REMOVE_ORPHANS_AFTER_DAYS)
            if (!QFile::remove(filePath))
                cerr << "error: failed to remove '" << filePath.toUtf8().data() << "'!" << endl;
    }

    // Remove all old sync files
    foreach (const QString file, syncPoolFiles) {
        // Check when it was last modified
        QString filePath = syncPoolPath + "/" + file;
        QFileInfo info(filePath);

        if (info.lastModified().daysTo(currentDateTime) >= BOXIT_REMOVE_ORPHANS_AFTER_DAYS)
            if (!QFile::remove(filePath))
                cerr << "error: failed to remove '" << filePath.toUtf8().data() << "'!" << endl;
    }


    QMutexLocker locker(&mutex);

    // Remove all locks
    for (int i = 0; i < branches.size(); ++i) {
        Branch *branch = branches[i];

        for (int i = 0; i < branch->repos.size(); ++i) {
            Repo *repo = branch->repos[i];

            if (repo->getLockedSessionID() == BOXIT_SYSTEM_SESSION_ID)
                repo->unlock();
        }
    }
}




//###
//### Private
//###


void Database::_keepOrphanFiles(QStringList & files, const QStringList & checkPackages) {
    const int sigLength = QString(BOXIT_SIGNATURE_ENDING).length();
    QString checkFileName;

    QStringList::iterator it = files.begin();

    while (it != files.end()) {
        checkFileName = *it;
        if (checkFileName.endsWith(BOXIT_SIGNATURE_ENDING))
            checkFileName.chop(sigLength);

        // Remove from list if it exists in list
        if (checkPackages.contains(checkFileName))
            it = files.erase(it);
        else
            ++it;
    }

    files.removeDuplicates();
}



Branch* Database::_getBranch(const QString branchName) {
    for (int i = 0; i < branches.size(); ++i) {
        Branch *branch = branches[i];

        if (branch->name != branchName)
            continue;

        return branch;
    }

    return NULL;
}



Repo* Database::_getRepo(const QString branchName, const QString repoName, const QString repoArchitecture) {
    for (int i = 0; i < branches.size(); ++i) {
        Branch *branch = branches[i];

        if (branch->name != branchName)
            continue;

        for (int i = 0; i < branch->repos.size(); ++i) {
            Repo *repo = branch->repos[i];

            if (repo->getName() == repoName && repo->getArchitecture() == repoArchitecture)
                return repo;
        }

        break;
    }

    return NULL;
}



void Database::_releasePoolLock(const int sessionID) {
    // Release locked pool session
    lockedPoolFiles.remove(sessionID);
}



void Database::_releaseRepoLock(const int sessionID) {
    // Release all locked repos
    for (int i = 0; i < branches.size(); ++i) {
        Branch *branch = branches[i];

        for (int i = 0; i < branch->repos.size(); ++i) {
            Repo *repo = branch->repos[i];

            if (repo->getLockedSessionID() != sessionID)
                continue;

            repo->unlock();
        }
    }
}
