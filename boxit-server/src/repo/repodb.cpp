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


QList<Repo*> RepoDB::repos;
QMutex RepoDB::mutex;



void RepoDB::initRepoDB() {
    QMutexLocker locker(&mutex);

    QString repoDir = Global::getConfig().repoDir;
    QStringList repoList = QDir(repoDir).entryList(QDir::AllDirs | QDir::NoDotAndDotDot, QDir::Name);
    QStringList architectures = QString(BOXIT_ARCHITECTURES).split(" ", QString::SkipEmptyParts);

    for (int i = 0; i < repoList.size(); ++i) {
        QString repo = repoList.at(i);

        for (int x = 0; x < architectures.size(); ++x) {
            QString architecture = architectures.at(x);

            // Remove base repo folder if emtpy
            if (!QDir().exists(repoDir + "/" + repo + "/" + architecture))
                cleanupBaseRepoDirIfRequired(repo);
            else if (!newRepo(repo, architecture))
                cerr << QString("warning: failed to add repo %1:%2!").arg(repo , architecture).toAscii().data() << endl;
        }
    }
}



void RepoDB::removeFilesWhichExistsInReposFromList(QStringList &files) {
    QMutexLocker locker(&mutex);
    QStringList notFoundFiles;

    for (int i = 0; i < files.size(); ++i) {
        QString filename = files.at(i);
        bool found = false;

        for (int i = 0; i < repos.size(); ++i) {
            QStringList packages = repos.at(i)->getPackages();

            for (int i = 0; i < packages.size(); ++i) {
                QString package = packages.at(i);

                if (package == filename || package + BOXIT_SIGNATURE_ENDING == filename) {
                    found = true;
                    break;
                }
            }

            if (found)
                break;
        }

        if (!found)
            notFoundFiles.append(filename);
    }

    files = notFoundFiles;
    files.removeDuplicates();
}



bool RepoDB::lockPool(QString username, QString name, QString architecture) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if ((repo = getRepo(name, architecture)) == NULL || Pool::isLocked(repo))
        return false;

    return Pool::lock(repo, username);
}




bool RepoDB::repoExists(QString name, QString architecture) {
    QMutexLocker locker(&mutex);
    return (getRepo(name, architecture) != NULL);
}



bool RepoDB::repoIsBusy(QString name, QString architecture) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if ((repo = getRepo(name, architecture)) == NULL)
        return false;

    return Pool::isLocked(repo);
}



QList<RepoDB::RepoInfo> RepoDB::getRepoList() {
    QMutexLocker locker(&mutex);
    QList<RepoDB::RepoInfo> infos;

    for (int i = 0; i < repos.size(); ++i) {
        Repo *repo = repos.at(i);

        RepoInfo info;
        info.name = repo->getName();
        info.architecture = repo->getArchitecture();

        infos.append(info);
    }

    return infos;
}



bool RepoDB::mergeRepos(QString username, QString srcName, QString destName, QString architecture) {
    QMutexLocker locker(&mutex);
    Repo *srcRepo, *destRepo;

    if ((srcRepo = getRepo(srcName, architecture)) == NULL
            || (destRepo = getRepo(destName, architecture)) == NULL
            || Pool::isLocked(srcRepo)
            || Pool::isLocked(destRepo)
            || !Pool::lock(destRepo, username))
        return false;

    QStringList srcPackages = srcRepo->getPackages();
    QStringList destPackages = destRepo->getPackages();

    // Set all packages to add
    for (int i = 0; i < srcPackages.size(); ++i) {
        QString srcPackage = srcPackages.at(i);
        bool found = false;

        for (int i = 0; i < destPackages.size(); ++i) {
            if (destPackages.at(i) == srcPackage) {
                found = true;
                break;
            }
        }

        if (!found)
            Pool::addFile(srcPackage);
    }

    // Set all packages to remove
    for (int i = 0; i < destPackages.size(); ++i) {
        QString destPackage = destPackages.at(i);
        bool found = false;

        for (int i = 0; i < srcPackages.size(); ++i) {
            if (srcPackages.at(i) == destPackage) {
                found = true;
                break;
            }
        }

        if (!found)
            Pool::removeFile(destPackage);
    }

    if (!Pool::commit()) {
        Pool::unlock();
        return false;
    }

    return true;
}



bool RepoDB::createRepo(QString name, QString architecture) {
    QMutexLocker locker(&mutex);

    // Check if repo already exists
    if (getRepo(name, architecture) != NULL)
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
    if (!newRepo(name, architecture))
        return false;

    return true;
}


bool RepoDB::changeRepoSyncURL(QString name, QString architecture, QString syncURL) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if ((repo = getRepo(name, architecture)) == NULL || Pool::isLocked(repo))
        return false;

    return repo->setSyncURL(syncURL);
}



bool RepoDB::getRepoSyncURL(QString name, QString architecture, QString &syncURL) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if ((repo = getRepo(name, architecture)) == NULL || Pool::isLocked(repo))
        return false;

    syncURL = repo->getSyncURL();
    return true;
}



bool RepoDB::repoSync(QString username, QString name, QString architecture, QString customSyncPackages) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if (Pool::isLocked() || (repo = getRepo(name, architecture)) == NULL || repo->getSyncURL().isEmpty() || !Pool::lock(repo, username))
        return false;

    return Pool::synchronize(customSyncPackages);
}




bool RepoDB::getRepoState(QString name, QString architecture, QString &state) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if ((repo = getRepo(name, architecture)) == NULL || Pool::isLocked(repo))
        return false;

    state = repo->getState();

    return true;
}



bool RepoDB::getRepoPackages(QString name, QString architecture, QStringList &packages) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if ((repo = getRepo(name, architecture)) == NULL || Pool::isLocked(repo))
        return false;

    packages = repo->getPackages();

    return true;
}



bool RepoDB::getRepoSignatures(QString name, QString architecture, QStringList &signatures) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if ((repo = getRepo(name, architecture)) == NULL || Pool::isLocked(repo))
        return false;

    signatures = repo->getSignatures();

    return true;
}



bool RepoDB::getRepoExcludeContent(QString name, QString architecture, QString &content) {
    QMutexLocker locker(&mutex);
    Repo *repo;

    if ((repo = getRepo(name, architecture)) == NULL || Pool::isLocked(repo))
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

    if ((repo = getRepo(name, architecture)) == NULL || Pool::isLocked(repo))
        return false;

    QFile file(repo->getPath() + "/" + repo->getSyncExcludeFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << content;
    file.close();

    return true;
}



bool RepoDB::poolKillThread() {
    QMutexLocker locker(&mutex);

    if (!Pool::isLocked())
        return false;

    Pool::killThread();
    return true;
}



QString RepoDB::poolMessageHistory() {
    QMutexLocker locker(&mutex);

    return Pool::getMessageHistory();
}





//###
//### Private
//###



void RepoDB::cleanupBaseRepoDirIfRequired(QString repoName) {
    QString path = QString(Global::getConfig().repoDir) + "/" + repoName;
    QDir dir(path);

    if (!dir.exists())
        return;

    // Remove base repo folder if emtpy
    if (dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty())
        Global::rmDir(path);
}



Repo* RepoDB::getRepo(QString name, QString architecture) {
    // Basic checks if repo name and arch is valid
    if (name.isEmpty() || architecture.isEmpty())
        return NULL;

    for (int i = 0; i < repos.size(); ++i) {
        Repo *repo = repos[i];

        if (repo->getName() == name && repo->getArchitecture() == architecture)
            return repo;
    }

    return NULL;
}



bool RepoDB::newRepo(QString name, QString architecture) {
    QString destPath = QString(Global::getConfig().repoDir) + "/" + name + "/" + architecture;

    if (!QDir().exists(destPath)) {
        // Remove base repo folder if emtpy
        cleanupBaseRepoDirIfRequired(name);
        return false;
    }

    if (getRepo(name, architecture) != NULL)
        return false;

    Repo *repo = new Repo(name, destPath, architecture);
    if (!repo->update()) {
        delete(repo);
        return false;
    }

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
