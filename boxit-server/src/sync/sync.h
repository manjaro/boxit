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

#ifndef SYNC_H
#define SYNC_H

#include <QThread>
#include <QString>
#include <QList>
#include <QStringList>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QEventLoop>
#include <QProcess>
#include <QRegExp>
#include <iostream>
#include <unistd.h>

#include "download.h"
#include "const.h"
#include "global.h"
#include "sha256/cryptsha256.h"
#include "db/branch.h"
#include "db/repo.h"
#include "db/status.h"

using namespace std;



class Sync : public QThread
{
    Q_OBJECT
public:
    explicit Sync(QObject *parent = 0);
    ~Sync();

    void abort();
    int start(const QString username, Branch *branch);
    QString lastErrorMessage() { return errorMessage; }

private:
    struct Package {
        QString packageName, fileName, sha256sum, url;
        bool downloadSignature, downloadPackage;
    };

    struct SyncRepo {
        Repo *repo;
        QStringList addPackages, removePackages;
    };


    Branch *branch;
    const QString tmpPath;
    int sessionID;
    QString errorMessage;

    void run();
    bool downloadSyncPackages(const QList<Package> & downloadPackages);
    bool getDownloadSyncPackages(QString url, const QString repoName, const QStringList & excludeFiles, QList<Package> & downloadPackages, QStringList & dbPackages);
    void cleanupTmpDir();
    bool downloadFile(const QString url, const QString destPath);
    bool fillPackagesList(const QString repoName, QList<Package> & packages);
    bool matchWithWildcard(const QString &str, const QStringList &list);

signals:
    void status(int index, int total);

};

#endif // SYNC_H
