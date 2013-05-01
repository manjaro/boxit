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

#ifndef BRANCH_H
#define BRANCH_H

#include <QObject>
#include <QString>
#include <QList>
#include <QStringList>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>
#include <unistd.h>

#include "global.h"
#include "const.h"
#include "repo.h"
#include "status.h"

using namespace std;


class Branch : QObject
{
    Q_OBJECT
public:
    const QString name, path;
    QList<Repo*> repos;

    Branch(const QString name, const QString path);
    ~Branch();

    bool init();
    bool setExcludeFilesContent(const QString content);
    bool setUrl(const QString url);

    QString getUrl() { return url; }
    QStringList getExcludeFiles() { return excludeFiles; }
    QString getExcludeFilesContent() { return excludeFilesContent; }

private:
    QMutex repoThreadMutex, setBranchStateMutex;
    QString url, excludeFilesContent;
    QStringList excludeFiles;

    bool readExcludeContentConfig();
    bool readConfig();
    bool updateConfig();

private slots:
    void setNewBranchState();
    void repoThreadFailed(Repo *repo, int threadSessionID);
    void repoThreadWaiting(Repo *repo, int threadSessionID);
    void repoThreadFinished(Repo *repo, int threadSessionID);

};

#endif // BRANCH_H

