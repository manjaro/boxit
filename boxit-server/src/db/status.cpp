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
    if (success)
        emit self.branchSessionFinished(sessionID);
    else
        emit self.branchSessionFailed(sessionID);
}



void Status::init() {
    timer.setInterval(60000);
    connect(&timer, SIGNAL(timeout())   ,   this, SLOT(timeout()));
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
