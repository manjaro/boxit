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

#include "repodb.h"


RepoDB RepoDB::self;
QList<Repo*> RepoDB::repos;
QMutex RepoDB::mutex;



void RepoDB::initRepoDB() {
    QMutexLocker locker(&mutex);
    QStringList repoList = QDir(Global::getConfig().repoDir).entryList(QDir::AllDirs | QDir::NoDotAndDotDot, QDir::Name);

    for (int i = 0; i < repoList.size(); ++i) {
        QStringList architectures = QString(BOXIT_ARCHITECTURES).split(" ", QString::SkipEmptyParts);

        for (int x = 0; x < architectures.size(); ++x) {
            if (!_newRepo(repoList.at(i), architectures.at(x)))
                cerr << "warning: failed to add repo " << repoList.at(i).toAscii().data() << ":" << architectures.at(x).toAscii().data() << "!" << endl;
        }
    }
}



Repo* RepoDB::lockRepo(QString name, QString architecture, QString username) {
    QMutexLocker locker(&mutex);

    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return NULL;

    repo->lock(username);
    return repo;
}



bool RepoDB::repoExists(QString name, QString architecture) {
    QMutexLocker locker(&mutex);
    return (_getRepo(name, architecture) != NULL);
}



bool RepoDB::repoIsBusy(QString name, QString architecture) {
    QMutexLocker locker(&mutex);
    Repo *repo = _getRepo(name, architecture);

    if (repo == NULL)
        return false;

    return (repo->isLocked() || repo->thread.isRunning());
}



QList<RepoDB::RepoInfo> RepoDB::getRepoList() {
    QMutexLocker locker(&mutex);
    QList<RepoDB::RepoInfo> infos;

    for (int i = 0; i < repos.size(); ++i) {
        Repo *repo = repos.at(i);

        RepoInfo info;
        info.name = repo->getName();
        info.architecture = repo->getArchitecture();
        info.threadJob = repo->getThreadJob();
        info.threadState = repo->getThreadState();

        infos.append(info);
    }

    return infos;
}



bool RepoDB::createRepo(QString name, QString architecture) {
    QMutexLocker locker(&mutex);

    if (!repoArgsValid(name, architecture) || _getRepo(name, architecture) != NULL)
        return false;


    // Create repo dir
    QString path = QString(Global::getConfig().repoDir) + "/" + name + "/" + architecture;
    if (!QDir().exists(path) && !QDir().mkpath(path))
        return false;


    // Create repo config
    QFile file(path + "/" + BOXIT_REPO_CONFIG);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "###\n### BoxIt Repository Config\n###\n";
    out << "\nstate = newboxitrepo";
    out << "\nsync =";
    file.close();


    // Create new repo
    if (!_newRepo(name, architecture))
        return false;

    return true;
}



bool RepoDB::removeRepo(QString username, QString name, QString architecture) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return false;

    repo->cleanupTMPDir();
    return repo->thread.start(username, RepoThread::JOB_REMOVE);
}



bool RepoDB::moveRepo(QString username, QString name, QString architecture, QString newName) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (newName.isEmpty() || _getRepo(newName, architecture) != NULL || !repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return false;

    repo->cleanupTMPDir();
    return repo->thread.start(username, RepoThread::JOB_MOVE, newName);
}



bool RepoDB::copyRepo(QString username, QString name, QString architecture, QString newName) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (newName.isEmpty() || _getRepo(newName, architecture) != NULL || !repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return false;

    repo->cleanupTMPDir();
    return repo->thread.start(username, RepoThread::JOB_COPY, newName);
}



bool RepoDB::changeRepoSyncURL(QString name, QString architecture, QString syncURL) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return false;

    return repo->setSyncURL(syncURL);
}



bool RepoDB::getRepoSyncURL(QString name, QString architecture, QString &syncURL) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return false;

    syncURL = repo->getSyncURL();
    return true;
}



bool RepoDB::repoRebuildDB(QString username, QString name, QString architecture) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return false;

    if (!repo->update())
        return false;

    return repo->thread.start(username, RepoThread::JOB_DB_REBUILD);
}



bool RepoDB::repoSync(QString username, QString name, QString architecture, QString customSyncPackages, bool updateRepoFirst) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning() || repo->getSyncURL().isEmpty())
        return false;

    if (updateRepoFirst && !repo->update())
        return false;

    return repo->thread.start(username, RepoThread::JOB_SYNC, customSyncPackages);
}



bool RepoDB::getRepoSyncInfo(QString name, QString architecture, bool &hasTMPSync) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return false;

    hasTMPSync = repo->hasTMPSync();

    return true;
}



bool RepoDB::getRepoState(QString name, QString architecture, QString &state) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return false;

    state = repo->getState();

    return true;
}



bool RepoDB::getRepoPackages(QString name, QString architecture, QStringList &packages) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return false;

    packages = repo->getPackages();

    return true;
}



bool RepoDB::getRepoSignatures(QString name, QString architecture, QStringList &signatures) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return false;

    signatures = repo->getSignatures();

    return true;
}



bool RepoDB::getRepoExcludeContent(QString name, QString architecture, QString &content) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return false;

    QFile file(repo->getPath() + "/" + repo->getSyncExcludeFile());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    content = in.readAll();
    file.close();

    return true;
}



bool RepoDB::setRepoExcludeContent(QString name, QString architecture, QString content) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || repo->thread.isRunning())
        return false;

    QFile file(repo->getPath() + "/" + repo->getSyncExcludeFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << content;
    file.close();

    return true;
}



bool RepoDB::repoKillThread(QString name, QString architecture) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL || repo->isLocked() || !repo->thread.isRunning())
        return false;

    repo->thread.kill();

    return true;
}



bool RepoDB::repoMessageHistory(QString name, QString architecture, QString &messageHistory) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (!repoArgsValid(name, architecture) || (repo = _getRepo(name, architecture)) == NULL)
        return false;

    messageHistory = repo->thread.getMessageHistory();

    return true;
}





//###
//### Private
//###



bool RepoDB::repoArgsValid(QString name, QString architecture) {
    return (!name.isEmpty()
            && !architecture.isEmpty()
            && QString(BOXIT_ARCHITECTURES).split(" ", QString::SkipEmptyParts).contains(architecture));
}



Repo* RepoDB::_getRepo(QString name, QString architecture) {
    for (int i = 0; i < repos.size(); ++i) {
        Repo *repo = repos[i];

        if (repo->getName() == name && repo->getArchitecture() == architecture)
            return repo;
    }

    return NULL;
}



bool RepoDB::_newRepo(QString name, QString architecture) {
    QString destPath = QString(Global::getConfig().repoDir) + "/" + name + "/" + architecture;

    if (!QDir().exists(destPath)) {
        // Remove base repo folder if emtpy
        if (QDir(QString(Global::getConfig().repoDir) + "/" + name).entryList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::Name).isEmpty())
            Global::rmDir(QString(Global::getConfig().repoDir) + "/" + name);

        return false;
    }

    if (_getRepo(name, architecture) != NULL)
        return false;

    Repo *repo = new Repo(name, destPath, architecture);
    if (!repo->update()) {
        delete(repo);
        return false;
    }

    QObject::connect(&repo->thread, SIGNAL(requestDeletion(RepoBase*))  ,   &self, SLOT(requestDeletion(RepoBase*)));
    QObject::connect(&repo->thread, SIGNAL(requestNewRepo(QString,QString))  ,   &self, SLOT(requestNewRepo(QString,QString)));
    QObject::connect(&repo->thread, SIGNAL(message(QString,QString,QString))    ,   &self, SIGNAL(message(QString,QString,QString)));
    QObject::connect(&repo->thread, SIGNAL(finished(QString,QString,bool))  ,   &self, SIGNAL(finished(QString,QString,bool)));

    repos.append(repo);
    qSort(repos.begin(), repos.end(), sortRepoLessThan);

    return true;
}



bool RepoDB::sortRepoLessThan(Repo *repo1, Repo *repo2) {
    QString repo1Name = repo1->getName().toLower();
    QString repo2Name = repo2->getName().toLower();

    if (repo1Name == repo2Name)
        return repo1->getArchitecture().toLower() < repo2->getArchitecture().toLower();

    return repo1Name < repo2Name;
}




//###
//### Private slots
//###




void RepoDB::requestDeletion(RepoBase *repo) {
    QMutexLocker locker(&mutex);

    // Remove base repo folder if emtpy
    if (QDir(QString(Global::getConfig().repoDir) + "/" + repo->getName()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::Name).isEmpty())
        Global::rmDir(QString(Global::getConfig().repoDir) + "/" + repo->getName());


    Repo *listRepo = _getRepo(repo->getName(), repo->getArchitecture());
    if (listRepo == NULL)
        return; // Shouldn't happen ;)

    repos.removeAll(listRepo);
    listRepo->thread.kill();
    listRepo->deleteLater();
}



void RepoDB::requestNewRepo(QString name, QString architecture) {
    QMutexLocker locker(&mutex);

    _newRepo(name, architecture);
}
