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

#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QList>
#include <QStringList>
#include <QMutex>
#include <QMutexLocker>
#include <QMap>
#include <iostream>

#include "global.h"
#include "const.h"
#include "branch.h"
#include "sync/sync.h"

using namespace std;


class Database
{
public:
    struct RepoInfo {
        QString name, architecture, state;
        bool isSyncRepo;
        QStringList overlayPackages, syncPackages;
    };

    static void init();

    static QStringList getBranches();
    static bool getBranchUrl(const QString branchName, QString & url);
    static bool setBranchUrl(const QString branchName, const QString url);
    static bool getBranchSyncExcludeFiles(const QString branchName, QString & excludeFilesContent);
    static bool setBranchSyncExcludeFiles(const QString branchName, const QString excludeFilesContent);

    static bool getRepos(const QString branchName, QList<Database::RepoInfo> & list);
    static bool lockRepo(const QString branchName, const QString repoName, const QString repoArchitecture, const int sessionID, const QString username);
    static bool adjustRepoFiles(const QString branchName, const QString repoName, const QString repoArchitecture, const int sessionID, const QStringList & addPackages, const QStringList & removePackages);
    static void releaseRepoLock(const int sessionID);

    static bool lockPoolFiles(const int sessionID, const QString username, const QStringList & files);
    static bool checkPoolFilesExists(const QStringList & files, QStringList & missingFiles);
    static bool getPoolFileCheckSum(const QString file, QByteArray & checkSum);
    static bool moveFilesToPool(const int sessionID, const QString path, QStringList files);
    static void releasePoolLock(const int sessionID);

    static bool synchronizeBranch(const QString branchName, const QString username, int & syncSessionID);

    static void releaseSession(const int sessionID);

    static void removeOrphanPoolFiles();

private:
    struct PoolLock {
        QString username;
        QStringList files;
    };

    static QMutex mutex;
    static Sync sync;
    static QList<Branch*> branches;
    static QMap<int, PoolLock> lockedPoolFiles;
    static QStringList poolFiles;

    static void _keepOrphanFiles(QStringList & files, const QStringList & checkPackages);
    static Branch* _getBranch(const QString branchName);
    static Repo* _getRepo(const QString branchName, const QString repoName, const QString repoArchitecture);
    static void _releaseRepoLock(const int sessionID);
    static void _releasePoolLock(const int sessionID);
};

#endif // DATABASE_H
