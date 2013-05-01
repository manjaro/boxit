/*
 *
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

#ifndef GLOBAL_H
#define GLOBAL_H

#include <QCoreApplication>
#include <QString>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QList>
#include <QStringList>
#include <QFileInfo>
#include <QTextStream>
#include <QCryptographicHash>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <unistd.h>
#include <iostream>
#include <sys/stat.h>

#include "const.h"

using namespace std;


class Global
{
public:
    struct Config {
        QString salt, sslCertificate, sslKey, repoDir, syncPoolDir, overlayPoolDir;
        QStringList mailingListEMails;
    };

    struct RepoChanges {
        QString branchName, repoName, repoArchitecture;
        QStringList addedPackages, removedPackages;
    };

    static int getNewUniqueSessionID();
    static QString getNameofPKG(QString pkg);
    static QString getVersionofPKG(QString pkg);
    static QByteArray sha1CheckSum(const QString filePath);
    static bool sendMemoEMail(const QString mailPrefixMessage, const QList<RepoChanges> & repoChanges);
    static bool sendMemoEMail(const QString mailMessage, const QStringList attachments);
    static bool sendEMail(const QString subject, const QString to, const QString text, const QStringList attachments);
    static bool rmDir(const QString path, const bool onlyHidden = false, const bool onlyContent = false);
    static bool copyDir(const QString src, const QString dst, const bool hidden = false);
    static QString getSymlinkTarget(const QString symlink);
    static bool fixFilePermission(const QString file);
    static bool setFilePermission(const QString file, const mode_t mode);
    static bool preserveDirectoryPermission(const QString srcDir, const QString destDir);
    static Config getConfig();
    static bool readConfig();

private:
    static Config config;
    static int lastSessionID;
    static QMutex newSessionIDMutex;

};

#endif // GLOBAL_H
