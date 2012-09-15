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

#ifndef REPODB_H
#define REPODB_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <iostream>
#include "repo.h"
#include "pool.h"
#include "const.h"

using namespace std;


class RepoDB : public QObject
{
    Q_OBJECT
public:
    struct RepoInfo {
        QString name, architecture;
    };

    static void initRepoDB();
    static bool fileExistsInRepos(QString filename);
    static bool lockPool(QString username, QString name, QString architecture);
    static bool repoExists(QString name, QString architecture);
    static bool repoIsBusy(QString name, QString architecture);
    static QList<RepoInfo> getRepoList();
    static bool mergeRepos(QString username, QString srcName, QString destName, QString architecture);
    static bool createRepo(QString name, QString architecture);
    static bool changeRepoSyncURL(QString name, QString architecture, QString syncURL);
    static bool getRepoSyncURL(QString name, QString architecture, QString &syncURL);
    static bool repoSync(QString username, QString name, QString architecture, QString customSyncPackages);
    static bool getRepoState(QString name, QString architecture, QString &state);
    static bool getRepoPackages(QString name, QString architecture, QStringList &packages);
    static bool getRepoSignatures(QString name, QString architecture, QStringList &signatures);
    static bool getRepoExcludeContent(QString name, QString architecture, QString &content);
    static bool setRepoExcludeContent(QString name, QString architecture, QString content);
    static bool poolKillThread();
    static QString poolMessageHistory();

private:
    static QList<Repo*> repos;
    static QMutex mutex;

    static void cleanupBaseRepoDirIfRequired(QString repoName);
    static Repo* getRepo(QString name, QString architecture);
    static bool newRepo(QString name, QString architecture);
    static bool sortRepoLessThan(Repo *repo1, Repo *repo2);

};

#endif // REPODB_H
