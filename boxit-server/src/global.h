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
#include "const.h"
#include <sys/stat.h>



class Global
{
public:
    struct Config {
        QString salt, sslCertificate, sslKey, repoDir, poolDir;
        QStringList mailingListEMails;
    };

    static QString getNameofPKG(QString pkg);
    static QString getVersionofPKG(QString pkg);
    static bool fixFilePermission(QString file) ;
    static bool sendMemoEMail(QString username, QString repository, QString architecture, QStringList addedFiles, QStringList removedFiles);
    static bool sendEMail(QString subject, QString to, QString text);
    static bool rmDir(QString path, bool onlyHidden = false, bool onlyContent = false);
    static bool copyDir(QString src, QString dst, bool hidden = false);
    static Config getConfig();
    static bool readConfig();

private:
    static Config config;
};

#endif // GLOBAL_H
