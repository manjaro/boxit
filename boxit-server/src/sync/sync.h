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

#include <QObject>
#include <QString>
#include <QList>
#include <QStringList>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QEventLoop>
#include <QProcess>
#include <QRegExp>
#include "download.h"
#include "const.h"
#include "global.h"
#include "sha256/cryptsha256.h"



class Sync : public QObject
{
    Q_OBJECT
public:
    explicit Sync(const QString destPath, QObject *parent = 0);
    ~Sync();

    bool synchronize(QString url, const QString repoName, const QString excludeFilePath, QStringList &allDBPackages, QStringList &addedFiles, QStringList checkFilePaths = QStringList(), QStringList onlyFiles = QStringList());

signals:
    void error(QString errorStr);
    void status(int index, int total);

private:
    struct Package {
        QString packageName, fileName, sha256sum;
        bool downloadSignature, downloadPackage;
    };

    const QString destPath;
    Download download;
    QList<Package> packages;
    bool busy;

    bool downloadFile(QString url);
    bool fillPackagesList(const QString repoName);
    bool fileAlreadyExist(Package &package, const QStringList &checkFilePaths);
    bool readExcludeFile(const QString filePath, QStringList &patterns);
    bool matchWithWildcard(const QString &str, const QStringList &list);
};

#endif // SYNC_H
