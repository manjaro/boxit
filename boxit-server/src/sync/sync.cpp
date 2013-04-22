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

#include "sync.h"


Sync::Sync(QObject *parent) :
    QThread(parent),
    tmpPath(QString(BOXIT_SESSION_TMP) + "/sync_session")
{
    branch = NULL;
    sessionID = -1;

    // Create tmp folder
    cleanupTmpDir();
}



Sync::~Sync() {
    abort();

    // Remove tmp path
    if (QDir(tmpPath).exists() && !Global::rmDir(tmpPath))
        cerr << "error: failed to remove sync session tmp path!" << endl;
}



void Sync::abort() {
    if (!isRunning())
        return;

    terminate();
    wait();

    // Unlock all locked repositories by this session ID
    if (branch) {
        for (int i = 0; i < branch->repos.size(); ++i) {
            Repo *repo = branch->repos[i];

            if (repo->getLockedSessionID() == sessionID)
                repo->unlock();
        }
    }
}



int Sync::start(const QString username, Branch *branch) {
    if (isRunning())
        return -1;

    errorMessage.clear();
    this->branch = branch;
    this->sessionID = Global::getNewUniqueSessionID();

    // Check if a repository is already locked
    for (int i = 0; i < branch->repos.size(); ++i) {
        if (branch->repos[i]->isLocked())
            return -1;
    }

    // Try to lock repositories
    for (int i = 0; i < branch->repos.size(); ++i) {
        if (branch->repos[i]->lock(sessionID, username))
            continue;

        // Unlock all locked repositories by this session ID on error
        for (int i = 0; i < branch->repos.size(); ++i) {
            Repo *repo = branch->repos[i];

            if (repo->getLockedSessionID() == sessionID)
                repo->unlock();
        }

        return -1;
    }

    QThread::start();

    return this->sessionID;
}



//###
//### Private methods
//###


void Sync::run() {
    // Update state
    Status::setBranchStateChanged(branch->name, "synchronizing packages", "", Status::STATE_RUNNING);

    // Wait a little to be sure the remote client is ready
    sleep(2);

    QList<Package> downloadPackages;
    QList<SyncRepo> syncRepos;
    bool noCommits = true;

    // Clean up tmp folder
    cleanupTmpDir();

    for (int i = 0; i < branch->repos.size(); ++i) {
        Repo *repo = branch->repos[i];

        // Skip if this is not a sync repo
        if (!repo->isSyncable())
            continue;

        QString url = branch->getUrl();
        url.replace("$branch", branch->name);
        url.replace("$repo", repo->getName());
        url.replace("$arch", repo->getArchitecture());

        QStringList dbPackages;
        SyncRepo syncRepo;
        syncRepo.repo = repo;

        // Get all packages to download
        if (!getDownloadSyncPackages(url, repo->getName(), branch->getExcludeFiles(), downloadPackages, dbPackages))
            goto error;


        QStringList repoSyncPackages = repo->getSyncPackages();

        // Get packages to add
        foreach (const QString package, dbPackages) {
            if (repoSyncPackages.contains(package))
                continue;

            syncRepo.addPackages.append(package);
        }

        // Get packages to remove
        foreach (const QString package, repoSyncPackages) {
            if (dbPackages.contains(package))
                continue;

            syncRepo.removePackages.append(package);
        }

        // Add to list
        syncRepos.append(syncRepo);
    }


    // Download all packages
    if (!downloadSyncPackages(downloadPackages))
        goto error;


    // Update state
    Status::setBranchStateChanged(branch->name, "committing changes", "", Status::STATE_RUNNING);


    // Commit changes
    for (int i = 0; i < syncRepos.size(); ++i) {
        SyncRepo *syncRepo = &syncRepos[i];

        // Skip if nothing is to do
        if (syncRepo->addPackages.isEmpty() && syncRepo->removePackages.isEmpty())
            continue;

        if (!syncRepo->repo->adjustPackages(syncRepo->addPackages, syncRepo->removePackages, true))
            goto error;

        noCommits = false;
    }

    // Unlock all locked repositories by this session ID
    for (int i = 0; i < branch->repos.size(); ++i) {
        Repo *repo = branch->repos[i];

        if (repo->getLockedSessionID() == sessionID)
            repo->unlock();
    }

    // Update state
    Status::setBranchStateChanged(branch->name, "finished synchronization", "", Status::STATE_SUCCESS);

    // If no commits have been done, then manually trigger signal
    if (noCommits)
        Status::branchSessionChanged(sessionID, true);

    branch = NULL;
    return;

error:

    // Unlock all locked repositories by this session ID
    for (int i = 0; i < branch->repos.size(); ++i) {
        Repo *repo = branch->repos[i];

        if (repo->getLockedSessionID() == sessionID)
            repo->unlock();
    }

    // Update state
    Status::setBranchStateChanged(branch->name, "synchronization failed", errorMessage, Status::STATE_FAILED);

    // If no commits have been done, then manually trigger signal
    if (noCommits)
        Status::branchSessionChanged(sessionID, false);

    branch = NULL;
}



bool Sync::downloadSyncPackages(const QList<Package> & downloadPackages) {
    const QString syncPath = Global::getConfig().syncPoolDir;

    // Download each file
    for (int i = 0; i < downloadPackages.size(); ++i) {
        const Package *package = &downloadPackages.at(i);

        // Update status
        emit status(i + 1, downloadPackages.size());
        Status::setBranchStateChanged(branch->name, "synchronizing packages [" + QString::number(i + 1) + "/" + QString::number(downloadPackages.size()) + "]", "", Status::STATE_RUNNING);

        // Download file...
        if (package->downloadPackage) {
            if (!downloadFile(package->url + package->fileName))
                return false;

            // Check if package checksum is ok
            if (CryptSHA256::sha256CheckSum(tmpPath + "/" + package->fileName) != package->sha256sum) {
                errorMessage = QString("error: package checksum doesn't match for package '%1'!").arg(package->fileName);
                return false;
            }

            // Wait a little bit before moving the file. Otherwise sometimes the move might fail...
            usleep(200000);

            // Move file to sync pool directory
            if (!QDir().rename(tmpPath + "/" + package->fileName, syncPath + "/" + package->fileName)) {
                errorMessage = QString("error: failed to move file '%1' to sync folder!").arg(package->fileName);
                return false;
            }

            // Fix file permission
            Global::fixFilePermission(syncPath + "/" + package->fileName);
        }

        // Download signature file...
        if (package->downloadSignature) {
            if (!downloadFile(package->url + package->fileName + BOXIT_SIGNATURE_ENDING)) {
                errorMessage = QString("error: failed to download signature of package '%1'!").arg(package->fileName);
                return false;
            }

            // Wait a little bit before moving the file. Otherwise sometimes the move might fail...
            usleep(200000);

            // Move file to sync pool directory
            if (!QDir().rename(tmpPath + "/" + package->fileName + BOXIT_SIGNATURE_ENDING, syncPath + "/" + package->fileName + BOXIT_SIGNATURE_ENDING)) {
                errorMessage = QString("error: failed to move file '%1' to sync folder!").arg(package->fileName + BOXIT_SIGNATURE_ENDING);
                return false;
            }

            // Fix file permission
            Global::fixFilePermission(syncPath + "/" + package->fileName + BOXIT_SIGNATURE_ENDING);
        }
    }

    return true;
}



bool Sync::getDownloadSyncPackages(QString url, const QString repoName, const QStringList & excludeFiles, QList<Package> & downloadPackages, QStringList & dbPackages) {
    QList<Package> packages;
    const QString syncPath = Global::getConfig().syncPoolDir;

    if (!url.endsWith("/"))
        url += "/";

    errorMessage.clear();

    // First download the database...
    if (!downloadFile(url + repoName + BOXIT_DB_ENDING))
        return false;

    // Fill the packages list
    if (!fillPackagesList(repoName, packages))
        return false;

    // Remove database file again
    QFile::remove(tmpPath + "/" + repoName + BOXIT_DB_ENDING); // Error isn't important

    // Clean up first
    dbPackages.clear();

    // Set which packages should be downloaded
    for (int i = 0; i < packages.size(); ++i) {
        Package package = packages.at(i);
        package.url = url;

        // Check if the file is blacklisted
        if (matchWithWildcard(package.packageName, excludeFiles))
            continue;

        // Add to db list
        dbPackages.append(package.fileName);

        // Check if file already exists
        if (QFile::exists(syncPath + "/" + package.fileName))
            package.downloadPackage = false;
        else
            package.downloadPackage = true;

        if (QFile::exists(syncPath + "/" + package.fileName + BOXIT_SIGNATURE_ENDING))
            package.downloadSignature = false;
        else
            package.downloadSignature = true;

        if (package.downloadPackage || package.downloadSignature)
            downloadPackages.append(package);
    }

    return true;
}



void Sync::cleanupTmpDir() {
    // Remove tmp path
    if (QDir(tmpPath).exists() && !Global::rmDir(tmpPath))
        cerr << "error: failed to remove session tmp path!" << endl;

    // Create tmp path
    if (!QDir().mkpath(tmpPath))
        cerr << "error: failed to create session tmp path!" << endl;
}



bool Sync::downloadFile(QString url) {
    Download download(tmpPath);
    QEventLoop eventLoop;
    QObject::connect(&download, SIGNAL(finished(bool)), &eventLoop, SLOT(quit()));

    if (!download.download(url)) {
        errorMessage = "error: failed to download file '" + url + "'!";
        return false;
    }

    eventLoop.exec();

    if (download.hasError()) {
        errorMessage = "error: failed to download file '" + url + "'!\nerror message: " + download.lastError();
        return false;
    }

    return true;
}



bool Sync::fillPackagesList(const QString repoName, QList<Package> & packages) {
    QString dbPath = tmpPath + "/.db";

    // Create working folder
    if ((QDir(dbPath).exists() && !Global::rmDir(dbPath))) {
        errorMessage = "error: failed to remove folder '" + dbPath + "'";
        return false;
    }

    if (!QDir().mkpath(dbPath)) {
        errorMessage = "error: failed to create folder '" + dbPath + "'";
        return false;
    }


    // Unpack the database file
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setWorkingDirectory(dbPath);
    process.start("tar", QStringList() << "-xf" << tmpPath + "/" + repoName + BOXIT_DB_ENDING << "-C" << dbPath);

    if (!process.waitForFinished(60000)) {
        errorMessage = "error: archive extract process timeout!";
        return false;
    }

    if (process.exitCode() != 0) {
        errorMessage = "error: archive extract process failed: " + QString(process.readAll());
        return false;
    }


    // Fill packages list
    packages.clear();
    QStringList list = QDir(dbPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (int i = 0; i < list.size(); ++i) {
        QString path = dbPath + "/" + list.at(i) + "/" + BOXIT_DB_DESC_FILE;

        // Read config if exist
        QFile file(path);
        if (!file.exists())
            continue;

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            errorMessage = QString("error: failed to read database file '%1'!").arg(path);
            return false;
        }

        Package package;

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString nextLine, line = in.readLine().trimmed();
            if (line.isEmpty() || in.atEnd() || (nextLine = in.readLine().trimmed()).isEmpty())
                continue;

            if (line == "%NAME%")
                package.packageName = nextLine;
            else if (line == "%FILENAME%")
                package.fileName = nextLine;
            else if (line == "%SHA256SUM%")
                package.sha256sum = nextLine;
        }
        file.close();

        if (package.packageName.isEmpty() || package.fileName.isEmpty() || package.sha256sum.isEmpty()) {
            errorMessage = QString("uncomplete desc file '%1'!").arg(path);
            return false;
        }

        packages.append(package);
    }

    // Cleanup
    Global::rmDir(dbPath); // Error isn't important...

    return true;
}



bool Sync::matchWithWildcard(const QString &str, const QStringList &patterns) {
    QRegExp rx;
    rx.setPatternSyntax(QRegExp::Wildcard);

    for (int i = 0; i < patterns.size(); ++i) {
        rx.setPattern(patterns.at(i));

        if (rx.exactMatch(str))
            return true;
    }

    return false;
}
