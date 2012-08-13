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
#include "repobase.h"
#include "repothread.h"
#include "const.h"

using namespace std;


class RepoDB : public QObject
{
    Q_OBJECT
public:
    struct RepoInfo {
        QString name, architecture, threadJob, threadState;
    };

    static RepoDB self;

    static void initRepoDB();
    static Repo* lockRepo(QString name, QString architecture, QString username);
    static bool repoExists(QString name, QString architecture);
    static bool repoIsBusy(QString name, QString architecture);
    static QList<RepoInfo> getRepoList();
    static bool createRepo(QString name, QString architecture);
    static bool removeRepo(QString username, QString name, QString architecture);
    static bool moveRepo(QString username, QString name, QString architecture, QString newName);
    static bool copyRepo(QString username, QString name, QString architecture, QString newName);
    static bool changeRepoSyncURL(QString name, QString architecture, QString syncURL);
    static bool getRepoSyncURL(QString name, QString architecture, QString &syncURL);
    static bool repoRebuildDB(QString username, QString name, QString architecture);
    static bool repoSync(QString username, QString name, QString architecture, QString customSyncPackages, bool updateRepoFirst);
    static bool getRepoSyncInfo(QString name, QString architecture, bool &hasTMPSync);
    static bool getRepoState(QString name, QString architecture, QString &state);
    static bool getRepoPackages(QString name, QString architecture, QStringList &packages);
    static bool getRepoSignatures(QString name, QString architecture, QStringList &signatures);
    static bool getRepoExcludeContent(QString name, QString architecture, QString &content);
    static bool setRepoExcludeContent(QString name, QString architecture, QString content);
    static bool repoKillThread(QString name, QString architecture);
    static bool repoMessageHistory(QString name, QString architecture, QString &messageHistory);

signals:
    void message(QString repository, QString architecture, QString msg);
    void finished(QString repository, QString architecture, bool success);

private:
    static QList<Repo*> repos;
    static QMutex mutex;

    static bool repoArgsValid(QString name, QString architecture);
    static Repo* _getRepo(QString name, QString architecture);
    static bool _newRepo(QString name, QString architecture);
    static bool sortRepoLessThan(Repo *repo1, Repo *repo2);

private slots:
    void requestDeletion(RepoBase *repo);
    void requestNewRepo(QString name, QString architecture);

};

#endif // REPODB_H
