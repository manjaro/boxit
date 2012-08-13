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

#ifndef REPOTHREAD_H
#define REPOTHREAD_H

#include <QObject>
#include <QThread>
#include <QString>
#include <QDir>
#include <QFile>
#include <unistd.h>
#include "repobase.h"
#include "global.h"
#include "sync/sync.h"



class RepoThread : public QThread
{
    Q_OBJECT
    Q_ENUMS(JOB)
public:
    enum JOB { JOB_NOTHING = -1, JOB_REMOVE, JOB_MOVE, JOB_COPY, JOB_DB_REBUILD, JOB_SYNC, JOB_COMMIT };

    explicit RepoThread(RepoBase *repo, QObject *parent = 0);

    bool start(QString username, JOB job, QString arg1 = "");
    void kill();
    void run();
    QString getMessageHistory();

signals:
    void finished(QString repository, QString architecture, bool success);
    void message(QString repository, QString architecture, QString msg);
    void requestDeletion(RepoBase *repo);
    void requestNewRepo(QString name, QString architecture);

private:
    QString username, arg1;
    QStringList messageHistory;
    RepoBase *repo;
    JOB job;

    inline void renameDB(const QString path, const QString oldName, const QString newName);
    bool removePackagesExceptList(const QString path, const QStringList &list, QStringList &removedFiles);
    void addToHistory(QString str);

private slots:
    void emitMessage(QString msg);
    void syncStatus(int index, int total);
};

#endif // REPOTHREAD_H
