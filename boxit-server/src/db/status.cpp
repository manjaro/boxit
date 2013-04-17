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

#include "status.h"



Status Status::self;
QMutex Status::mutex;
QList<Status::BranchStatus> Status::branches;
QList<Status::RepoStatus> Status::repos;
QList<Status::RepoCommit> Status::repoCommits;




Status::Status()
{
    timer.setInterval(60000); // 60 sec

    commitTimer.setInterval(5000); // 5 sec
    commitTimer.setSingleShot(true);

    // Connect signals and slots
    connect(&timer, SIGNAL(timeout())   ,   this, SLOT(timeout()));
    connect(&commitTimer, SIGNAL(timeout())   ,   this, SLOT(commitTimeout()));
}



void Status::setRepoCommit(const QString username, const QString branchName, const QString name, const QString architecture, const QStringList & addPackages, const QStringList & removePackages) {
    QMutexLocker locker(&mutex);

    // Find item if already in list
    for (int i = 0; i < repoCommits.size(); ++i) {
        RepoCommit *repo = &repoCommits[i];

        if (repo->username != username || repo->branchName != branchName || repo->name != name || repo->architecture != architecture)
            continue;

        repo->addPackages = addPackages;
        repo->removePackages = removePackages;

        // Restart timer
        self.restartCommitTimer();

        return;
    }

    // Create item if not already in list
    RepoCommit repo;
    repo.username = username;
    repo.branchName = branchName;
    repo.name = name;
    repo.architecture = architecture;
    repo.addPackages = addPackages;
    repo.removePackages = removePackages;

    repoCommits.append(repo);

    // Restart timer
    self.restartCommitTimer();
}



void Status::setRepoStateChanged(const QString branchName, const QString name, const QString architecture, const QString job, const QString error, const Status::STATE state) {
    QMutexLocker locker(&mutex);

    // Find item if already in list
    for (int i = 0; i < repos.size(); ++i) {
        RepoStatus *repo = &repos[i];

        if (repo->branchName != branchName || repo->name != name || repo->architecture != architecture)
            continue;

        repo->job = job;
        repo->error = error;
        repo->state = state;
        repo->timeout = 0;

        // Trigger signal
        emit self.stateChanged();

        return;
    }

    // Create item if not already in list
    RepoStatus repo;
    repo.branchName = branchName;
    repo.name = name;
    repo.architecture = architecture;
    repo.job = job;
    repo.error = error;
    repo.state = state;
    repo.timeout = 0;

    repos.append(repo);

    // Trigger signal
    emit self.stateChanged();
}



void Status::setBranchStateChanged(const QString name, const QString job, const QString error, const Status::STATE state) {
    QMutexLocker locker(&mutex);

    // Find item if already in list
    for (int i = 0; i < branches.size(); ++i) {
        BranchStatus *branch = &branches[i];

        if (branch->name != name)
            continue;

        branch->job = job;
        branch->error = error;
        branch->state = state;
        branch->timeout = 0;

        // Trigger signal
        emit self.stateChanged();

        return;
    }

    // Create item if not already in list
    BranchStatus branch;
    branch.name = name;
    branch.job = job;
    branch.error = error;
    branch.state = state;
    branch.timeout = 0;

    branches.append(branch);

    // Trigger signal
    emit self.stateChanged();
}



QList<Status::RepoStatus> Status::getReposStatus() {
    QMutexLocker locker(&mutex);

    return repos;
}



QList<Status::BranchStatus> Status::getBranchesStatus() {
    QMutexLocker locker(&mutex);

    return branches;
}



void Status::branchSessionChanged(const int sessionID, bool success) {
    QMutexLocker locker(&mutex);

    if (success)
        emit self.branchSessionFinished(sessionID);
    else
        emit self.branchSessionFailed(sessionID);
}



void Status::init() {
    self.initSelf();
}



void Status::restartCommitTimer() {
    commitTimer.stop();
    commitTimer.start();
}



void Status::initSelf() {
    timer.start();
}



void Status::timeout() {
    QMutexLocker locker(&mutex);

    bool changed = false;

    QList<RepoStatus>::iterator iRepo = repos.begin();
    while (iRepo != repos.end()) {
        // Increase timeout count
        (*iRepo).timeout += 1;

        // Remove from list on timeout
        if (((*iRepo).state == STATE_FAILED && (*iRepo).timeout > 600)          // Timeout on error after 10 hours
                || ((*iRepo).state == STATE_RUNNING && (*iRepo).timeout > 300)  // Timeout while running after 5 hours
                || ((*iRepo).state == STATE_WAITING && (*iRepo).timeout > 300)  // Timeout while waiting after 5 hours
                || ((*iRepo).state == STATE_SUCCESS && (*iRepo).timeout > 30)   // Timeout on success after 30 minutes
                || (*iRepo).timeout > 600)                                      // Default: timeout after 10 hours
        {
            iRepo = repos.erase(iRepo);
            changed = true;
        }
        else {
            ++iRepo;
        }
    }


    QList<BranchStatus>::iterator iBranch = branches.begin();
    while (iBranch != branches.end()) {
        // Increase timeout count
        (*iBranch).timeout += 1;

        // Remove from list on timeout
        if (((*iBranch).state == STATE_FAILED && (*iBranch).timeout > 600)          // Timeout on error after 10 hours
                || ((*iBranch).state == STATE_RUNNING && (*iBranch).timeout > 300)  // Timeout while running after 5 hours
                || ((*iBranch).state == STATE_WAITING && (*iBranch).timeout > 300)  // Timeout while waiting after 5 hours
                || ((*iBranch).state == STATE_SUCCESS && (*iBranch).timeout > 30)   // Timeout on success after 30 minutes
                || (*iBranch).timeout > 600)                                        // Default: timeout after 10 hours
        {
            iBranch = branches.erase(iBranch);
            changed = true;
        }
        else {
            ++iBranch;
        }
    }

    // Trigger signal if something has changed
    if (changed)
        emit stateChanged();
}



void Status::commitTimeout() {
    QMutexLocker locker(&mutex);

    // Sort
    QMap<QString, QList<RepoCommit*> > map;

    for (int i = 0; i < repoCommits.size(); ++i) {
        RepoCommit *repo = &repoCommits[i];

        // Add to map
        map[repo->username].append(repo);
    }

    // Send e-mails
    QMapIterator<QString, QList<RepoCommit*> > it(map);
    while (it.hasNext()) {
        it.next();

        QStringList attachments;

        // Create working folder
        if ((QDir(BOXIT_STATUS_TMP).exists() && !Global::rmDir(BOXIT_STATUS_TMP))) {
            cerr << "error: failed to remove folder '" << BOXIT_STATUS_TMP << "'" << endl;
            continue;
        }

        if (!QDir().mkpath(BOXIT_STATUS_TMP)) {
            cerr << "error: failed to create folder '" << BOXIT_STATUS_TMP << "'" << endl;
            continue;
        }

        QString message = "### BoxIt memo ###\n\n";
        message += QString("User %1 committed following changes:\n\n").arg(it.key());

        for (int i = 0; i < it.value().size(); ++i) {
            RepoCommit *repo = it.value()[i];

            message += QString(" - %1 %2 %3:\t%4 new and %5 removed package(s)\n").arg(repo->branchName, repo->name, repo->architecture,
                                                                                    QString::number(repo->addPackages.size()),
                                                                                    QString::number(repo->removePackages.size()));

            // Create attachment file
            QFile file(QString(BOXIT_STATUS_TMP) + "/" + repo->branchName + "_" + repo->name + "_" + repo->architecture);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                continue;

            QTextStream out(&file);
            if (!repo->addPackages.isEmpty()) {
                out << "[New Packages]\n" << repo->addPackages.join("\n");

                if (!repo->removePackages.isEmpty())
                    out << "\n\n\n";
            }

            if (!repo->removePackages.isEmpty())
                out << "[Removed Packages]\n" << repo->removePackages.join("\n");

            file.close();

            // Add to list
            attachments.append(file.fileName());
        }


        // Send e-mail
        if (!Global::sendMemoEMail(message, attachments))
            cerr << "error: failed to send e-mail!" << endl;
    }

    // Clean up
    repoCommits.clear();
}
