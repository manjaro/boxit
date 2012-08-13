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
    loginCount = 0;
    timeoutCount = 0;
    lockedRepo = NULL;
    timeOutTimer.setInterval(5000);

    // Connect signals and slots
    connect(this, SIGNAL(readData(quint16,QByteArray))  ,   this, SLOT(read_Data(quint16,QByteArray)));
    connect(&timeOutTimer, SIGNAL(timeout())    ,   this, SLOT(timeOutDestroy()));
    connect(&RepoDB::self, SIGNAL(message(QString,QString,QString)) ,   this, SLOT(sendMessage(QString,QString,QString)));
    connect(&RepoDB::self, SIGNAL(finished(QString,QString,bool))   ,   this, SLOT(sendFinished(QString,QString,bool)));

    timeOutTimer.start();
}



BoxitInstance::~BoxitInstance() {
    if (file.isOpen()) {
        file.close();
        if (file.exists())
            file.remove();
        file.setFileName("");
    }

    if (lockedRepo != NULL) {
        lockedRepo->unlock();
        lockedRepo = NULL;
    }
}



//###
//### Slots
//###



void BoxitInstance::sendFinished(QString repository, QString architecture, bool success) {
    if (messageRepoName.isEmpty() || messageRepoName != repository || messageRepoArchitecture != architecture)
        return;

    if (success)
        sendData(MSG_PROCESS_FINISHED);
    else
        sendData(MSG_PROCESS_FAILED);
}



void BoxitInstance::sendMessage(QString repository, QString architecture, QString msg) {
    if (messageRepoName.isEmpty() || messageRepoName != repository || messageRepoArchitecture != architecture)
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
        QStringList request = QString(data).split(BOXIT_SPLIT_CHAR, QString::SkipEmptyParts);

        if (request.size() < 2) {
            sendData(MSG_ERROR);
            break;
        }
        else if (lockedRepo != NULL) {
            sendData(MSG_ERROR_REPO_LOCK_ONLY_ONCE);
            break;
        }
        else if (!RepoDB::repoExists(request.at(0), request.at(1))) {
            sendData(MSG_ERROR_NOT_EXIST);
            break;
        }

        lockedRepo = RepoDB::lockRepo(request.at(0), request.at(1), user.getUsername());
        if (lockedRepo == NULL) {
            sendData(MSG_ERROR_REPO_ALREADY_LOCKED);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_COMMIT:
    {
        if (lockedRepo == NULL) {
            sendData(MSG_ERROR_REPO_NOT_LOCKED);
            break;
        }

        if (!lockedRepo->thread.start(user.getUsername(), RepoThread::JOB_COMMIT))
            sendData(MSG_ERROR);
        else
            sendData(MSG_SUCCESS);

        break;
    }
    case MSG_REMOVE_PACKAGE:
    {
        if (lockedRepo == NULL) {
            sendData(MSG_ERROR_REPO_NOT_LOCKED);
            break;
        }

        lockedRepo->removeFile(QString(data));

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_FILE_UPLOAD:
    {
        if (lockedRepo == NULL) {
            sendData(MSG_ERROR_REPO_NOT_LOCKED);
            break;
        }

        if (file.isOpen()) {
            file.close();
            if (file.exists())
                file.remove();
            file.setFileName("");
        }

        file.setFileName(lockedRepo->getWorkPath() + "/" + QString(data).split("/", QString::SkipEmptyParts).last());

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

        if (!data.isEmpty() && data != sha1CheckSum(file.fileName())) {
            file.remove();
            file.setFileName("");
            sendData(MSG_ERROR_CHECKSUM_WRONG);
            break;
        }

        if (lockedRepo == NULL) {
            sendData(MSG_ERROR);
            break;
        }


        file.setFileName("");
        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_UNLOCK:
    {
        if (lockedRepo == NULL) {
            sendData(MSG_ERROR_REPO_NOT_LOCKED);
            break;
        }

        lockedRepo->unlock();
        lockedRepo = NULL;
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
            sendData(MSG_LIST_REPO, QByteArray(QString(info.name + BOXIT_SPLIT_CHAR + info.architecture + BOXIT_SPLIT_CHAR + info.threadJob + BOXIT_SPLIT_CHAR + info.threadState).toAscii()));
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
    case MSG_REPO_REMOVE:
    {
        QStringList split;
        if (!basicRepoChecks(data, split))
            break;

        if (!RepoDB::removeRepo(user.getUsername(), split.at(0), split.at(1))) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_REPO_COPY:
    {
        QStringList split;
        if (!basicRepoChecks(data, split, 3))
            break;

        if (RepoDB::repoExists(split.at(2), split.at(1))) {
            sendData(MSG_ERROR_ALREADY_EXIST);
            break;
        }
        else if (!RepoDB::copyRepo(user.getUsername(), split.at(0), split.at(1), split.at(2))) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_REPO_MOVE:
    {
        QStringList split;
        if (!basicRepoChecks(data, split, 3))
            break;

        if (RepoDB::repoExists(split.at(2), split.at(1))) {
            sendData(MSG_ERROR_ALREADY_EXIST);
            break;
        }
        else if (!RepoDB::moveRepo(user.getUsername(), split.at(0), split.at(1), split.at(2))) {
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
    case MSG_REPO_DB_REBUILD:
    {
        QStringList split;
        if (!basicRepoChecks(data, split))
            break;

        if (!RepoDB::repoRebuildDB(user.getUsername(), split.at(0), split.at(1))) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_REPO_SYNC:
    {
        QStringList split;
        if (!basicRepoChecks(data, split, 3))
            break;

        QString syncURL, customSyncPackages;
        if (!RepoDB::getRepoSyncURL(split.at(0), split.at(1), syncURL)) {
            sendData(MSG_ERROR);
            break;
        }

        if (syncURL.isEmpty()) {
            sendData(MSG_ERROR_NO_SYNC_REPO);
            break;
        }

        if (split.size() >= 4)
            customSyncPackages = split.at(3);

        if (!RepoDB::repoSync(user.getUsername(), split.at(0), split.at(1), customSyncPackages, split.at(2).toInt())) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_REPO_HAS_TMP_SYNC:
    {
        QStringList split;
        if (!basicRepoChecks(data, split))
            break;

        bool hasTMPSync;

        if (!RepoDB::getRepoSyncInfo(split.at(0), split.at(1), hasTMPSync)) {
            sendData(MSG_ERROR);
            break;
        }

        if (hasTMPSync)
            sendData(MSG_YES);
        else
            sendData(MSG_NO);

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
        QStringList split = QString(data).split(QString(BOXIT_SPLIT_CHAR), QString::SkipEmptyParts);

        if (split.size() < 2) {
            sendData(MSG_ERROR);
            break;
        }
        else if (!RepoDB::repoExists(split.at(0), split.at(1))) {
            sendData(MSG_ERROR_NOT_EXIST);
            break;
        }
        else if (!RepoDB::repoIsBusy(split.at(0), split.at(1))) {
            sendData(MSG_ERROR_NOT_BUSY);
            break;
        }
        else if (!RepoDB::repoKillThread(split.at(0), split.at(1))) {
            sendData(MSG_ERROR);
            break;
        }

        sendData(MSG_SUCCESS);
        break;
    }
    case MSG_GET_REPO_MESSAGES:
    {
        QStringList split = QString(data).split(QString(BOXIT_SPLIT_CHAR), QString::SkipEmptyParts);
        QString messageHistory;

        if (split.size() < 2) {
            sendData(MSG_ERROR);
            break;
        }
        else if (!RepoDB::repoExists(split.at(0), split.at(1))) {
            sendData(MSG_ERROR_NOT_EXIST);
            break;
        }
        else if (!RepoDB::repoMessageHistory(split.at(0), split.at(1), messageHistory)) {
            sendData(MSG_ERROR);
            break;
        }
        else if (!RepoDB::repoIsBusy(split.at(0), split.at(1))) {
            sendData(MSG_ERROR_NOT_BUSY, QByteArray(messageHistory.toAscii()));
            break;
        }


        sendData(MSG_SUCCESS, QByteArray(messageHistory.toAscii()));
        messageRepoName = split.at(0);
        messageRepoArchitecture = split.at(1);
        break;
    }
    case MSG_STOP_REPO_MESSAGES:
    {
        messageRepoName.clear();
        messageRepoArchitecture.clear();
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
