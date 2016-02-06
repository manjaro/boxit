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

#ifndef REPO_H
#define REPO_H

#include <QCoreApplication>
#include <QThread>
#include <QString>
#include <QList>
#include <QStringList>
#include <QCryptographicHash>
#include <QDateTime>
#include <QWaitCondition>
#include <QMutex>
#include <iostream>
#include <unistd.h>
#include <sys/stat.h>

#include "global.h"
#include "const.h"
#include "status.h"


using namespace std;


class Repo : public QThread
{
    Q_OBJECT
public:
    Repo(const QString branchName, const QString name, const QString architecture, const QString path);
    ~Repo();

    bool init();
    bool setNewRandomState();
    bool adjustPackages(const QStringList & addPackages, const QStringList & removePackages, bool workWithSyncPackage = false);
    bool lock(const int sessionID, const QString username);
    void unlock();
    bool isLocked();
    bool commit();
    void abort();

    QString getName()           { return name; }
    QString getPath()           { return path; }
    QString getArchitecture()   { return architecture; }
    QString getLockedUsername() { return lockedUsername; }
    int getLockedSessionID()    { return lockedSessionID; }
    int getThreadSessionID()    { return threadSessionID; }
    bool isSyncable()           { return isSyncRepo; }
    bool waitingForCommit()     { return waitingCommit; }

    QString getState()                  { QMutexLocker locker(&mutexUpdatingRepoAttributes); return state; }
    QStringList getOverlayPackages()    { QMutexLocker locker(&mutexUpdatingRepoAttributes); return overlayPackages; }
    QStringList getSyncPackages()       { QMutexLocker locker(&mutexUpdatingRepoAttributes); return syncPackages; }

public slots:
    void start();

signals:
    void requestNewBranchState();
    void threadFailed(Repo *repo, int threadSessionID);
    void threadWaiting(Repo *repo, int threadSessionID);
    void threadFinished(Repo *repo, int threadSessionID);

protected:
    void run();

private:
    struct Package {
        QString name, version, file, link;
        bool isOverlayPackage;

        Package() {
            isOverlayPackage = false;
        }
    };

    const QString branchName, name, architecture, path, tmpPath, repoDB, repoDBLink, repoFiles, repoFilesLink;
    QString state, lockedUsername, threadUsername, threadErrorString;
    int lockedSessionID, threadSessionID;
    bool isSyncRepo, waitingCommit, isCommitting;
    QStringList overlayPackages, syncPackages;
    QStringList tmpOverlayPackages, tmpSyncPackages;
    QWaitCondition waitCondition;
    QMutex mutexWaitCondition, mutexUpdatingRepoAttributes;

    bool cleanupTmpDir();
    bool readConfig();
    bool updateConfig();
    bool readPackagesConfig(const QString fileName, QStringList & packages);
    bool writePackagesConfig(const QString fileName, const QStringList & packages);

    bool applySymlinks(const QList<Package> & packages, const QString path, const QString rootLink);
    bool symlinkExists(const QString path);

    bool updatePackageDatabase(const QList<Package> & packages);
    bool addPackagesToDatabase(const QStringList & packages);
    bool removePackagesFromDatabase(const QStringList & packages);
    bool updateFilesToDatabase(const QStringList & packages);
};

#endif // REPO_H
