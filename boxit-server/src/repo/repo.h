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

#include <QString>
#include <QStringList>
#include <QList>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <QCryptographicHash>
#include "const.h"
#include "global.h"


class Repo
{
public:
    explicit Repo(QString name, QString path, QString architecture);
    bool operator==(const Repo &repo);

    bool update();
    void updateName(QString name);
    bool setSyncURL(QString url);
    bool setNewRandomState();

    QString getName() { return name; }
    QString getPath() { return path; }
    QString getRepoDB() { return repoDB; }
    QString getRepoDBLink() { return repoDBLink; }
    QString getSyncURL() { return syncURL; }
    QString getSyncExcludeFile() { return syncExcludeFile; }
    QString getArchitecture() { return architecture; }
    QStringList getPackages() { return packages; }
    QStringList getSignatures() { return signatures; }
    QString getState() { return state; }


protected:
    QString name, path, architecture, repoDB, repoDBLink, syncURL, syncExcludeFile, state;
    QStringList packages, signatures;

private:
    bool readConfig();
    bool updateConfig();
};

#endif // REPO_H
