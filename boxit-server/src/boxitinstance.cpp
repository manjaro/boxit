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

#include "boxitinstance.h"


BoxitInstance::BoxitInstance(const int sessionID) :
    BoxitSocket(sessionID),
    tmpPath(QString(BOXIT_SESSION_TMP) + "/" + QString::number(sessionID))
{
    loginCount = 0;
    listenOnStatus = false;
    syncSessionID = -1;

    // Create tmp dir
    cleanupTmpDir();

    // Connect signals and slots
    connect(this, SIGNAL(readData(quint16,QByteArray))  ,   this, SLOT(read_Data(quint16,QByteArray)));
    connect(&Status::self, SIGNAL(stateChanged())   ,   this, SLOT(sendStatus()));
    connect(&Status::self, SIGNAL(branchSessionFailed(int))   ,   this, SLOT(statusBranchSessionFailed(int)));
    connect(&Status::self, SIGNAL(branchSessionFinished(int))   ,   this, SLOT(statusBranchSessionFinished(int)));
}



BoxitInstance::~BoxitInstance() {
    listenOnStatus = false;

    // Close and remove file if it is still open
    if (file.isOpen()) {
        file.close();
        if (file.exists())
            file.remove();
        file.setFileName("");
    }

    // Release session
    Database::releaseSession(sessionID);

    // Remove tmp path
    if (QDir(tmpPath).exists() && !Global::rmDir(tmpPath))
        cerr << "error: failed to remove session tmp path!" << endl;
}



void BoxitInstance::cleanupTmpDir() {
    // Remove tmp path
    if (QDir(tmpPath).exists() && !Global::rmDir(tmpPath))
        cerr << "error: failed to remove session tmp path!" << endl;

    // Create tmp path
    if (!QDir().mkpath(tmpPath))
        cerr << "error: failed to create session tmp path!" << endl;
}



//###
//### Slots
//###


void BoxitInstance::read_Data(quint16 msgID, QByteArray data) {
    //###
    //### Public accessible requests
    //###

    switch (msgID) {
    case MSG_CHECK_VERSION:
    {
        // Compare client and server version
        if ((int)data.toInt() != (int)BOXIT_VERSION) {
            sendData(MSG_ERROR);
            disconnectFromHost();
            return;
        }

        sendData(MSG_SUCCESS);
        return;
    }
    case MSG_AUTHENTICATE:
    {
        // Against some brute force attacks...
        sleep(1);

        ++loginCount;
        if (loginCount >= 3) {
            sendData(MSG_ERROR);
            disconnectFromHost();
            return;
        }

        QStringList split = QString(data).split(BOXIT_SPLIT_CHAR);
        if (split.size() < 2 || !UserDatabase::loginUser(split.at(0), split.at(1), user)) {
            sendData(MSG_ERROR);
            return;
        }

        sendData(MSG_SUCCESS);
        return;
    }
    }


    if (!user.isAuthorized()) {
        sendData(MSG_ERROR_NOT_AUTHORIZED);
        disconnectFromHost();
        return;
    }



    //###
    //### Private section - only authorized
    //###


    switch (msgID) {
    case MSG_GET_BRANCHES:
    {
        QStringList branches = Database::getBranches();

        foreach (QString branch, branches) {
            sendData(MSG_DATA_BRANCH, QByteArray(branch.toAscii()));
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_GET_REPOS:
    case MSG_GET_REPOS_WITH_PACKAGES:
    {
        QList<Database::RepoInfo> repos;

        if (!Database::getRepos(QString(data), repos)) {
            sendData(MSG_ERROR);
            break;
        }

        for (int i = 0; i < repos.size(); ++i) {
            const Database::RepoInfo *repo = &repos.at(i);

            // Send repository informations
            QString str = repo->name;
            str += BOXIT_SPLIT_CHAR + repo->architecture;
            str += BOXIT_SPLIT_CHAR + repo->state;
            str += BOXIT_SPLIT_CHAR + QString::number((int)repo->isSyncRepo);

            sendData(MSG_DATA_REPO, QByteArray(str.toAscii()));

            if (msgID == MSG_GET_REPOS_WITH_PACKAGES) {
                // Send all overlay packages
                sendStringList(MSG_DATA_OVERLAY_PACKAGES, repo->overlayPackages);

                // Send all sync packages
                sendStringList(MSG_DATA_SYNC_PACKAGES, repo->syncPackages);
            }
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_POOL_CHECK_FILES_EXISTS:
    {
        QStringList missingFiles;

        if (!Database::checkPoolFilesExists(QString(data).split(BOXIT_SPLIT_CHAR, QString::SkipEmptyParts), missingFiles))
            sendData(MSG_ERROR, QByteArray(missingFiles.join(BOXIT_SPLIT_CHAR).toAscii()));
        else
            sendData(MSG_SUCCESS);

        break;
    }
    case MSG_LOCK_POOL_FILES:
    {
        if (!Database::lockPoolFiles(sessionID, user.getUsername(), QString(data).split(BOXIT_SPLIT_CHAR, QString::SkipEmptyParts)))
            sendData(MSG_ERROR);
        else
            sendData(MSG_SUCCESS);

        break;
    }
    case MSG_MOVE_POOL_FILES: {
        if (!Database::moveFilesToPool(sessionID, tmpPath, uploadedFiles)) {
            sendData(MSG_ERROR);
            break;
        }

        uploadedFiles.clear();

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_RELEASE_POOL_LOCK: {
        Database::releasePoolLock(sessionID);
        uploadedFiles.clear();

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_LOCK_REPO: {
        QStringList list = QString(data).split(BOXIT_SPLIT_CHAR, QString::SkipEmptyParts);
        if (list.size() < 3) {
            sendData(MSG_ERROR);
            break;
        }

        if (!Database::lockRepo(list.at(0), list.at(1), list.at(2), sessionID, user.getUsername())) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_RELEASE_REPO_LOCK:
    {
        Database::releaseRepoLock(sessionID);

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_APPLY_REPO_CHANGES: {
        QStringList split = QString(data).split("\n", QString::KeepEmptyParts);
        if (split.size() < 3) {
            sendData(MSG_ERROR);
            break;
        }

        QStringList list = split.at(0).split(BOXIT_SPLIT_CHAR, QString::SkipEmptyParts);
        QStringList addPackages = split.at(1).split(BOXIT_SPLIT_CHAR, QString::SkipEmptyParts);
        QStringList removePackages = split.at(2).split(BOXIT_SPLIT_CHAR, QString::SkipEmptyParts);

        if (list.size() < 3) {
            sendData(MSG_ERROR);
            break;
        }

        if (!Database::adjustRepoFiles(list.at(0), list.at(1), list.at(2), sessionID, addPackages, removePackages)) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_LISTEN_ON_STATUS:
    {
        listenOnStatus = true;
        sendData(MSG_SUCCESS);
        sendStatus();
        break;
    }
    case MSG_STOP_LISTEN_ON_STATUS:
    {
        listenOnStatus = false;

        // Wait a little...
        sleep(1);

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_SYNC_BRANCH:
    {
        const QString branchName = QString(data);

        if (Database::isBranchLocked(branchName)) {
            sendData(MSG_IS_LOCKED);
            break;
        }

        if (!Database::synchronizeBranch(branchName, user.getUsername(), syncSessionID)) {
            syncSessionID = -1;
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_GET_BRANCH_URL:
    {
        QString url;

        if (!Database::getBranchUrl(QString(data), url)) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS, QByteArray(url.toAscii()));
        break;
    }
    case MSG_SET_BRANCH_URL:
    {
        QStringList split = QString(data).split(BOXIT_SPLIT_CHAR, QString::SkipEmptyParts);
        if (split.size() < 2) {
            sendData(MSG_ERROR);
            break;
        }

        if (!Database::setBranchUrl(split.at(0), split.at(1))) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_GET_BRANCH_SYNC_EXCLUDE_FILES:
    {
        QString excludeFiles;

        if (!Database::getBranchSyncExcludeFiles(QString(data), excludeFiles)) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS, QByteArray(excludeFiles.toAscii()));
        break;
    }
    case MSG_SET_BRANCH_SYNC_EXCLUDE_FILES:
    {
        QStringList split = QString(data).split(BOXIT_SPLIT_CHAR, QString::KeepEmptyParts);
        if (split.size() < 2) {
            sendData(MSG_ERROR);
            break;
        }

        QString branchName = split.at(0);
        split.removeFirst();

        if (!Database::setBranchSyncExcludeFiles(branchName, split.join(BOXIT_SPLIT_CHAR))) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_SET_PASSWD:
    {
        QStringList split = QString(data).split(BOXIT_SPLIT_CHAR, QString::SkipEmptyParts);

        if (split.size() < 2) {
            sendData(MSG_ERROR);
            break;
        }
        else if (!user.comparePassword(split.at(0))) {
            sendData(MSG_ERROR_WRONG_PASSWORD);
            break;
        }
        else if (!user.setPassword(split.at(1)) || !UserDatabase::setUserData(user)) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_SNAP_BRANCH:
    {
        QStringList split = QString(data).split(BOXIT_SPLIT_CHAR, QString::SkipEmptyParts);
        if (split.size() < 2) {
            sendData(MSG_ERROR);
            break;
        }

        if (Database::isBranchLocked(split.at(0)) || Database::isBranchLocked(split.at(1))) {
            sendData(MSG_IS_LOCKED);
            break;
        }

        if (!Database::snapshotBranch(split.at(0), split.at(1), user.getUsername(), sessionID)) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_FILE_CHECKSUM:
    {
        if (data.isEmpty()) {
            sendData(MSG_ERROR);
            break;
        }

        fileCheckSum = data;
        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_FILE_UPLOAD:
    {
        if (fileCheckSum.isEmpty()) {
            sendData(MSG_ERROR);
            break;
        }

        if (file.isOpen()) {
            file.close();
            if (file.exists())
                file.remove();
            file.setFileName("");
        }


        QString fileName = QString(data).split("/", QString::SkipEmptyParts).last();
        QByteArray poolFileCheckSum;

        // Check if file already exists
        if (Database::getPoolFileCheckSum(fileName, poolFileCheckSum)) {
            if (fileCheckSum != poolFileCheckSum) {
                sendData(MSG_FILE_ALREADY_EXISTS_WRONG_CHECKSUM);
                break;
            }
            else {
                sendData(MSG_FILE_ALREADY_EXISTS);
                break;
            }
        }

        file.setFileName(tmpPath + "/" + fileName);

        // Check if file already exists and remove it from the tmp path if required
        if (file.exists())
            file.remove();

        if (!file.open(QIODevice::WriteOnly)) {
            sendData(MSG_ERROR);
            file.close();
            file.setFileName("");
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_DATA_UPLOAD:
    {
        if (file.isOpen())
            file.write(data);

        break;
    }
    case MSG_DATA_UPLOAD_FINISH:
    {
        if (!file.isOpen()) {
            sendData(MSG_ERROR);
            break;
        }

        file.close();

        if (fileCheckSum != Global::sha1CheckSum(file.fileName())) {
            fileCheckSum.clear();
            file.remove();
            file.setFileName("");
            sendData(MSG_ERROR_WRONG_CHECKSUM);
            break;
        }

        // Add to list
        uploadedFiles.append(file.fileName().split("/", QString::SkipEmptyParts).last());

        fileCheckSum.clear();
        file.setFileName("");
        sendData(MSG_SUCCESS);
        break;
    }
    default:
    {
        sendData(MSG_INVALID);
        disconnectFromHost();
        break;
    }
    }
}



//###
//### Private
//###


void BoxitInstance::sendStringList(const quint16 msgID, const QStringList & list) {
    QString data;
    int count = 0;

    foreach (const QString item, list) {
        if (count < 50) {
            data.append(item + BOXIT_SPLIT_CHAR);
            ++count;
        }
        else {
            count = 0;
            data.append(item);
            sendData(msgID, QByteArray(data.toAscii()));
            data.clear();
        }
    }

    if (!data.isEmpty()) {
        if (data.endsWith(BOXIT_SPLIT_CHAR))
            data.remove(data.size() - 1, 1);

        sendData(msgID, QByteArray(data.toAscii()));
    }
}



void BoxitInstance::sendStatus() {
    QMutexLocker locker(&statusMutex);

    if (!listenOnStatus)
        return;

    QList<Status::BranchStatus> branches = Status::getBranchesStatus();
    QList<Status::RepoStatus> repos = Status::getReposStatus();

    sendData(MSG_DATA_NEW_STATUS_LIST);

    // Send branch status
    for (int i = 0; i < branches.size(); ++i) {
        const Status::BranchStatus *branch = &branches.at(i);

        QStringList list;
        list << branch->name;
        list << branch->job;
        list << QString::number((int)(branch->state == Status::STATE_FAILED));
        list << branch->error;

        sendData(MSG_DATA_BRANCH_STATUS, QByteArray(list.join(BOXIT_SPLIT_CHAR).toAscii()));
    }

    // Send repo status
    for (int i = 0; i < repos.size(); ++i) {
        const Status::RepoStatus *repo = &repos.at(i);

        QStringList list;
        list << repo->branchName;
        list << repo->name;
        list << repo->architecture;
        list << repo->job;
        list << QString::number((int)(repo->state == Status::STATE_FAILED));
        list << repo->error;

        sendData(MSG_DATA_REPO_STATUS, QByteArray(list.join(BOXIT_SPLIT_CHAR).toAscii()));
    }

    sendData(MSG_DATA_END_STATUS_LIST);
}



void BoxitInstance::statusBranchSessionFinished(int sessionID) {
    QMutexLocker locker(&statusMutex);

    if (!listenOnStatus || (this->sessionID != sessionID && this->syncSessionID != sessionID))
        return;

    sendData(MSG_STATUS_SESSION_FINISHED);
}



void BoxitInstance::statusBranchSessionFailed(int sessionID) {
    QMutexLocker locker(&statusMutex);

    if (!listenOnStatus || (this->sessionID != sessionID && this->syncSessionID != sessionID))
        return;

    sendData(MSG_STATUS_SESSION_FAILED);
}
