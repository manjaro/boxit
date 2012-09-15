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

#ifndef POOL_H
#define POOL_H

#include <QObject>
#include <QThread>
#include <QString>
#include <QDir>
#include <QFile>
#include <unistd.h>
#include "const.h"
#include "repo.h"
#include "global.h"
#include "sync/sync.h"


class Pool : public QThread
{
    Q_OBJECT
public:
    static Pool self;

    static bool isLocked();
    static bool isLocked(Repo *repo);
    static bool lock(Repo *repo, QString username);
    static bool lock();
    static void unlock();

    static void killThread();
    static QString getMessageHistory();

    static void addFile(QString file);
    static void removeFile(QString file);
    static bool commit();
    static bool synchronize(QString &customPackages);

    static QString getUsername() { return lockedUsername; }
    static QString getState() { return self.stateString; }
    static QString getJob() { return self.jobString; }
    static bool isProcessRunning() { return self.isRunning(); }
    static bool isProcessSuccess() { return self.processSuccess; }


    Pool();
    void kill();
    void run();

signals:
    void finished(bool success);
    void message(QString msg);

protected:
    enum JOB { JOB_SYNC, JOB_COMMIT };

    static bool locked;
    static Repo *lockedRepo;
    static QString lockedUsername;
    static QStringList removeFiles, addFiles;

    JOB job;
    QString stateString, jobString, arg1;
    QStringList messageHistory;
    bool processSuccess;

    void addToHistory(QString str);
    bool updatePackageDatabase();
    bool addPackagesToDatabase(const QStringList &packages);
    bool removePackagesFromDatabase(const QStringList &packages);

private slots:
    void emitMessage(QString msg);
    void syncStatus(int index, int total);

};

#endif // POOL_H
