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

#ifndef REPOBASE_H
#define REPOBASE_H

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


class RepoBase
{
public:
    explicit RepoBase(QString name, QString path, QString architecture);
    ~RepoBase();
    bool operator==(const RepoBase &repo);

    bool update();
    bool cleanupTMPDir(bool recreate = false);
    void updateName(QString name);
    bool setSyncURL(QString url);
    bool setNewRandomState();
    void removeFile(QString filename);
    void addFile(QString filename);

    QString getName() { return name; }
    QString getWorkPath() { return workPath; }
    QString getPath() { return path; }
    QString getRepoDB() { return repoDB; }
    QString getRepoDBLink() { return repoDBLink; }
    QString getSyncURL() { return syncURL; }
    QString getSyncExcludeFile() { return syncExcludeFile; }
    QString getArchitecture() { return architecture; }
    QStringList getPackages() { return packages; }
    QStringList getSignatures() { return signatures; }
    QStringList getRemovePackages() { return removePackages; }
    QString getState() { return state; }
    bool isLocked() { return locked; }
    void lock(QString username);
    void unlock();
    void forceLock() { locked = true; }
    void setThreadState(QString str) { threadState = str; }
    void setThreadJob(QString str) { threadJob = str; }
    QString getThreadState() { return threadState; }
    QString getThreadJob() { return threadJob; }
    bool hasTMPSync() { return tmpSyncAvailable; }
    void setTMPSync(bool hasSync) { tmpSyncAvailable = hasSync; }

protected:
    QString name, path, workPath, architecture, repoDB, repoDBLink, syncURL, syncExcludeFile, state, lockedUsername, threadState, threadJob;
    QStringList packages, signatures, removePackages;
    bool locked, tmpSyncAvailable;

private:
    bool readConfig();
    bool updateConfig();
};

#endif // REPOBASE_H
