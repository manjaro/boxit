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


BoxitInstance::BoxitInstance()
{
    sendMessages = false;
    poolLocked = false;
    loginCount = 0;
    timeoutCount = 0;
    timeOutTimer.setInterval(5000);

    // Connect signals and slots
    connect(this, SIGNAL(readData(quint16,QByteArray))  ,   this, SLOT(read_Data(quint16,QByteArray)));
    connect(&timeOutTimer, SIGNAL(timeout())    ,   this, SLOT(timeOutDestroy()));
    connect(&Pool::self, SIGNAL(message(QString)) ,   this, SLOT(sendMessage(QString)));
    connect(&Pool::self, SIGNAL(finished(bool))   ,   this, SLOT(sendFinished(bool)));

    timeOutTimer.start();
}



BoxitInstance::~BoxitInstance() {
    if (file.isOpen()) {
        file.close();
        if (file.exists())
            file.remove();
        file.setFileName("");
    }

    if (poolLocked) {
        poolLocked = false;
        Pool::unlock();
    }
}



//###
//### Slots
//###



void BoxitInstance::sendFinished(bool success) {
    if (!sendMessages)
        return;

    if (success)
        sendData(MSG_PROCESS_FINISHED);
    else
        sendData(MSG_PROCESS_FAILED);
}



void BoxitInstance::sendMessage(QString msg) {
    if (!sendMessages)
        return;

    sendData(MSG_MESSAGE, QByteArray(msg.toAscii()));
}



void BoxitInstance::timeOutDestroy() {
    timeoutCount += 5;

    if (timeoutCount >= 120) {
        timeoutCount = 0;
        disconnectFromHost();
    }
}



void BoxitInstance::read_Data(quint16 msgID, QByteArray data) {
    // Restart our timeout
    timeoutCount = 0;


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
        if (split.size() < 2) {
            sendData(MSG_ERROR);
            return;
        }
        else if (!UserDatabase::loginUser(split.at(0), split.at(1), user)) {
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
    case MSG_RESET_TIMEOUT:
    {
        break;
    }
    case MSG_LOCK:
    {
        if (Pool::isLocked()) {
            sendData(MSG_ERROR_POOL_ALREADY_LOCKED);
            break;
        }

        QStringList split;
        if (!basicRepoChecks(data, split))
            break;

        if (!RepoDB::lockPool(user.getUsername(), split.at(0), split.at(1))) {
            sendData(MSG_ERROR);
            break;
        }

        poolLocked = true;
        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_COMMIT:
    {
        if (!poolLocked) {
            sendData(MSG_ERROR_POOL_NOT_LOCKED);
            break;
        }

        if (!Pool::commit())
            sendData(MSG_ERROR);
        else
            sendData(MSG_SUCCESS);

        break;
    }
    case MSG_REMOVE_PACKAGE:
    {
        if (!poolLocked) {
            sendData(MSG_ERROR_POOL_NOT_LOCKED);
            break;
        }

        Pool::removeFile(QString(data).split("/", QString::SkipEmptyParts).last());

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
        if (!poolLocked) {
            sendData(MSG_ERROR_POOL_NOT_LOCKED);
            break;
        }
        else if (fileCheckSum.isEmpty()) {
            sendData(MSG_ERROR);
            break;
        }

        if (file.isOpen()) {
            file.close();
            if (file.exists())
                file.remove();
            file.setFileName("");
        }

        file.setFileName(Global::getConfig().poolDir + "/" + QString(data).split("/", QString::SkipEmptyParts).last());

        // Check if file exists and compare the checksums. Remove if different...
        if (file.exists()) {
            if (fileCheckSum != sha1CheckSum(file.fileName())) {
                file.remove();
            }
            else {
                Pool::addFile(file.fileName().split("/", QString::SkipEmptyParts).last());
                sendData(MSG_FILE_ALREADY_EXISTS);
                break;
            }
        }

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

        if (fileCheckSum != sha1CheckSum(file.fileName())) {
            fileCheckSum.clear();
            file.remove();
            file.setFileName("");
            sendData(MSG_ERROR_CHECKSUM_WRONG);
            break;
        }

        Pool::addFile(file.fileName().split("/", QString::SkipEmptyParts).last());
        fileCheckSum.clear();
        file.setFileName("");
        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_UNLOCK:
    {
        if (!poolLocked) {
            sendData(MSG_ERROR_POOL_NOT_LOCKED);
            break;
        }

        Pool::unlock();
        sendData(MSG_SUCCESS);
        break;
    }
    //###
    //### Section without a repo lock required
    //###
    case MSG_REPOSITORY_STATE:
    {
        QStringList split;
        if (!basicRepoChecks(data, split))
            break;

        QString state;
        if (!RepoDB::getRepoState(split.at(0), split.at(1), state)) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS, QByteArray(state.toAscii()));
        break;
    }
    case MSG_POOL_STATE:
    {
        sendData(MSG_SUCCESS, QByteArray(QString(Pool::getJob() + BOXIT_SPLIT_CHAR + Pool::getState() + BOXIT_SPLIT_CHAR + Pool::getUsername()).toAscii()));
        break;
    }
    case MSG_REQUEST_PROCESS_STATE:
    {
        if (Pool::isProcessRunning())
            sendData(MSG_PROCESS_RUNNING);
        else if (Pool::isProcessSuccess())
            sendData(MSG_PROCESS_FINISHED);
        else
            sendData(MSG_PROCESS_FAILED);

        break;
    }
    case MSG_PACKAGE_LIST:
    {
        QStringList split;
        if (!basicRepoChecks(data, split))
            break;

        QStringList packages;
        if (!RepoDB::getRepoPackages(split.at(0), split.at(1), packages)) {
            sendData(MSG_ERROR);
            break;
        }

        for (int i = 0; i < packages.size(); ++i)
            sendData(MSG_PACKAGE, QByteArray(packages.at(i).toAscii()));

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_SIGNATURE_LIST:
    {
        QStringList split;
        if (!basicRepoChecks(data, split))
            break;

        QStringList signatures;
        if (!RepoDB::getRepoSignatures(split.at(0), split.at(1), signatures)) {
            sendData(MSG_ERROR);
            break;
        }

        for (int i = 0; i < signatures.size(); ++i)
            sendData(MSG_SIGNATURE, QByteArray(signatures.at(i).toAscii()));

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_SET_PASSWD:
    {
        QStringList split = QString(data).split(QString(BOXIT_SPLIT_CHAR), QString::SkipEmptyParts);

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
    case MSG_REQUEST_LIST:
    {
        QList<RepoDB::RepoInfo> infos = RepoDB::getRepoList();

        for (int i = 0; i < infos.size(); ++i) {
            RepoDB::RepoInfo &info = infos[i];
            sendData(MSG_LIST_REPO, QByteArray(QString(info.name + BOXIT_SPLIT_CHAR + info.architecture).toAscii()));
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_REPO_ADD:
    {
        QStringList split = QString(data).split(QString(BOXIT_SPLIT_CHAR), QString::SkipEmptyParts);

        if (split.size() < 2) {
            sendData(MSG_ERROR);
            break;
        }
        else if (RepoDB::repoExists(split.at(0), split.at(1))) {
            sendData(MSG_ERROR_ALREADY_EXIST);
            break;
        }
        else if (RepoDB::repoIsBusy(split.at(0), split.at(1))) {
            sendData(MSG_ERROR_IS_BUSY);
            break;
        }
        else if (!RepoDB::createRepo(split.at(0), split.at(1))) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_REPO_GET_SYNC_URL:
    {
        QStringList split;
        if (!basicRepoChecks(data, split))
            break;

        QString syncURL;
        if (!RepoDB::getRepoSyncURL(split.at(0), split.at(1), syncURL)) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS, QByteArray(syncURL.toAscii()));
        break;
    }
    case MSG_REPO_CHANGE_SYNC_URL:
    {
        QStringList split;
        if (!basicRepoChecks(data, split, 3))
            break;

        if (!split.at(2).contains("@") || !RepoDB::changeRepoSyncURL(split.at(0), split.at(1), split.at(2))) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_REPO_MERGE:
    {
        QStringList split;
        if (!basicRepoChecks(data, split, 3))
            break;

        if (!RepoDB::repoExists(split.at(2), split.at(1))) {
            sendData(MSG_ERROR_NOT_EXIST);
            break;
        }
        else if (RepoDB::repoIsBusy(split.at(2), split.at(1))) {
            sendData(MSG_ERROR_IS_BUSY);
            break;
        }
        else if (Pool::isLocked()) {
            sendData(MSG_ERROR_POOL_ALREADY_LOCKED);
            break;
        }

        if (!RepoDB::mergeRepos(user.getUsername(), split.at(0), split.at(2), split.at(1))) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_REPO_SYNC:
    {
        QStringList split;
        if (!basicRepoChecks(data, split))
            break;

        if (Pool::isLocked()) {
            sendData(MSG_ERROR_POOL_ALREADY_LOCKED);
            break;
        }

        QString syncURL, customSyncPackages;
        if (!RepoDB::getRepoSyncURL(split.at(0), split.at(1), syncURL)) {
            sendData(MSG_ERROR);
            break;
        }

        if (syncURL.isEmpty()) {
            sendData(MSG_ERROR_NO_SYNC_REPO);
            break;
        }

        if (split.size() >= 3)
            customSyncPackages = split.at(2);

        if (!RepoDB::repoSync(user.getUsername(), split.at(0), split.at(1), customSyncPackages)) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_GET_EXCLUDE_CONTENT:
    {
        QStringList split;
        if (!basicRepoChecks(data, split))
            break;

        QString content;
        if (!RepoDB::getRepoExcludeContent(split.at(0), split.at(1), content)) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS, QByteArray(content.toAscii()));
        break;
    }
    case MSG_SET_EXCLUDE_CONTENT:
    {
        QStringList split;
        if (!basicRepoChecks(data, split))
            break;

        // It could be, that the content contains our boxit split char...
        QString repo = split.at(0);
        QString arch = split.at(1);
        split.removeFirst();
        split.removeFirst();

        if (!RepoDB::setRepoExcludeContent(repo, arch, split.join(BOXIT_SPLIT_CHAR))) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_KILL:
    {
        if (!Pool::isLocked()) {
            sendData(MSG_ERROR_NOT_BUSY);
            break;
        }
        else if (!RepoDB::poolKillThread()) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_GET_POOL_MESSAGES:
    {
        sendData(MSG_SUCCESS, QByteArray(RepoDB::poolMessageHistory().toAscii()));
        sendMessages = true;
        break;
    }
    case MSG_STOP_POOL_MESSAGES:
    {
        sendMessages = false;
        sendData(MSG_PROCESS_BACKGROUND);
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



bool BoxitInstance::basicRepoChecks(QByteArray &data, QStringList &split, int minimumLenght) {
    split = QString(data).split(QString(BOXIT_SPLIT_CHAR), QString::SkipEmptyParts);

    if (minimumLenght < 2)
        minimumLenght = 2;

    if (split.size() < minimumLenght) {
        sendData(MSG_ERROR);
        return false;
    }
    else if (!RepoDB::repoExists(split.at(0), split.at(1))) {
        sendData(MSG_ERROR_NOT_EXIST);
        return false;
    }
    else if (RepoDB::repoIsBusy(split.at(0), split.at(1))) {
        sendData(MSG_ERROR_IS_BUSY);
        return false;
    }

    return true;
}



QByteArray BoxitInstance::sha1CheckSum(QString filePath) {
    QCryptographicHash crypto(QCryptographicHash::Sha1);
    QFile file(filePath);

    if (!file.open(QFile::ReadOnly))
        return QByteArray();

    while(!file.atEnd()){
        crypto.addData(file.read(8192));
    }

    return crypto.result();
}
