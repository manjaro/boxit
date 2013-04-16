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

#ifndef STATUS_H
#define STATUS_H

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>



class Status : public QObject
{
    Q_OBJECT
public:
    enum STATE {
        STATE_RUNNING,
        STATE_WAITING,
        STATE_SUCCESS,
        STATE_FAILED
    };

    struct BranchStatus {
        QString name, job, error;
        STATE state;
        int timeout;
    };

    struct RepoStatus {
        QString branchName, name, architecture, job, error;
        STATE state;
        int timeout;
    };


    static void setRepoStateChanged(const QString branchName, const QString name, const QString architecture, const QString job, const QString error, const Status::STATE state);
    static void setBranchStateChanged(const QString name, const QString job, const QString error, const Status::STATE state);
    static QList<Status::RepoStatus> getReposStatus();
    static QList<Status::BranchStatus> getBranchesStatus();
    static void branchSessionChanged(const int sessionID, bool success);


    static Status self;

    void init();

signals:
    void stateChanged();
    void branchSessionFinished(int sessionID);
    void branchSessionFailed(int sessionID);

private:
    static QMutex mutex;
    static QList<BranchStatus> branches;
    static QList<RepoStatus> repos;
    
    QTimer timer;

private slots:
    void timeout();

};

#endif // STATUS_H
