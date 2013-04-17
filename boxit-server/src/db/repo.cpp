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

#include "repo.h"

Repo::Repo(const QString branchName, const QString name, const QString architecture, const QString path) :
    QThread(),
    branchName(branchName),
    name(name),
    architecture(architecture),
    path(path),
    tmpPath(QString(BOXIT_TMP) + "/" + branchName + "_" + name + "_" + architecture),
    repoDB(name + QString(BOXIT_DB_ENDING)),
    repoDBLink(name + QString(BOXIT_DB_LINK_ENDING))
{
    moveToThread(qApp->thread());
    setParent(qApp);

    isCommitting = false;
    isSyncRepo = false;
    waitingCommit = false;
    lockedSessionID = -1;
    threadSessionID = -1;

    // Create tmp folder if required
    cleanupTmpDir();
}



Repo::~Repo() {
    if (QDir(tmpPath).exists())
        Global::rmDir(tmpPath); // Error is not important
}



bool Repo::init() {
    // Read config
    if (!readConfig())
        return false;

    // Cleanup first
    overlayPackages.clear();
    syncPackages.clear();
    tmpOverlayPackages.clear();
    tmpSyncPackages.clear();

    // Get all overlay packages
    if (!readPackagesConfig(".overlaypackages", overlayPackages))
        return false;

    // Get all sync packages if this is a sync repo
    if (isSyncRepo && !readPackagesConfig(".syncpackages", syncPackages))
        return false;

    return true;
}



bool Repo::setNewRandomState() {
    // Create new state
    state = QString(QCryptographicHash::hash(QString(state + QDateTime::currentDateTime().toString(Qt::ISODate) + QString::number(qrand())).toLocal8Bit(), QCryptographicHash::Sha1).toHex());
    return updateConfig();
}



bool Repo::adjustPackages(const QStringList & addPackages, const QStringList & removePackages, bool workWithSyncPackage) {
    if (isRunning())
        return false;

    // Cleanup tmp
    if (!cleanupTmpDir())
        return false;

    // Create a temporary list ot overlay files
    tmpOverlayPackages = overlayPackages;
    tmpSyncPackages = syncPackages;

    QStringList *workPackages;
    if (workWithSyncPackage)
        workPackages = &tmpSyncPackages;
    else
        workPackages = &tmpOverlayPackages;


    // First remove files from lists
    foreach (QString removePackage, removePackages)
        workPackages->removeAll(removePackage);

    // Add all new files
    workPackages->append(addPackages);

    // Remove duplicates
    workPackages->removeDuplicates();

    // Start thread
    start();

    return true;
}



bool Repo::lock(const int sessionID, const QString username) {
    if (isLocked())
        return false;

    lockedSessionID = sessionID;
    lockedUsername = username;

    return true;
}



void Repo::unlock() {
    lockedSessionID = -1;
    lockedUsername.clear();
}



bool Repo::isLocked() {
    return (lockedSessionID >= 0 || isRunning());
}



void Repo::start() {
    isCommitting = false;
    waitingCommit = false;
    threadErrorString.clear();
    threadSessionID = lockedSessionID;
    threadUsername = lockedUsername;
    QThread::start();
}



bool Repo::commit() {
    if (!waitingCommit)
        return false;

    // Wake up thread
    waitCondition.wakeAll();

    return true;
}


void Repo::abort() {
    if (!isRunning() || isCommitting)
        return;

    // Terminate thread
    terminate();
    wait();

    mutexWaitCondition.unlock();
    waitingCommit = false;
    threadSessionID = -1;
    threadUsername.clear();

    // Status update
    Status::setRepoStateChanged(branchName, name, architecture, "process canceled", "Process canceled due to a process failure of another repository process with the same session ID.", Status::STATE_FAILED);
}



//###
//### Private
//###



void Repo::run() {
    // Status update
    Status::setRepoStateChanged(branchName, name, architecture, "applying changes", "", Status::STATE_RUNNING);

    // Wait a little to be sure the remote client is ready
    sleep(2);

    QList<Package> packages;
    QStringList addPackages, removePackages;

    // Add overlay packages
    foreach (const QString package, tmpOverlayPackages) {
        Package pkg;
        pkg.file = package;
        pkg.name = Global::getNameofPKG(package);
        pkg.version = Global::getVersionofPKG(package);
        pkg.link = QString(BOXIT_OVERLAY_POOL) + "/" + package;
        pkg.isOverlayPackage = true;

        packages.append(pkg);
    }

    // Add sync packages
    foreach (const QString package, tmpSyncPackages) {
        Package pkg;
        pkg.file = package;
        pkg.name = Global::getNameofPKG(package);
        pkg.version = Global::getVersionofPKG(package);
        pkg.link = QString(BOXIT_SYNC_POOL) + "/" + package;
        pkg.isOverlayPackage = false;

        // Check if package with same name is already in list - overlay package overwrite sync packages
        bool found = false;
        for (int i = 0; i < packages.size(); ++i) {
            if (pkg.name == packages.at(i).name) {
                found = true;
                break;
            }
        }

        if (!found)
            packages.append(pkg);
    }


    // Get a list of all added and removed packages
    {
        QStringList allPackages = syncPackages;
        allPackages.append(overlayPackages);

        QStringList allTmpPackages = tmpSyncPackages;
        allTmpPackages.append(tmpOverlayPackages);

        // Added packages
        foreach (const QString package, allTmpPackages) {
            if (!allPackages.contains(package))
                addPackages.append(package);
        }

        // Removed packages
        foreach (const QString package, allPackages) {
            if (!allTmpPackages.contains(package))
                removePackages.append(package);
        }

        addPackages.removeDuplicates();
        removePackages.removeDuplicates();
        addPackages.sort();
        removePackages.sort();
    }



    // Create symlinks
    if (!applySymlinks(packages, tmpPath, Global::getConfig().repoDir))
        goto error;

    // Copy database - error isn't critical
    if (QFile::exists(path + "/" + repoDB))
        QFile::copy(path + "/" + repoDB, tmpPath + "/" + repoDB);

    // Status update
    Status::setRepoStateChanged(branchName, name, architecture, "building package database", "", Status::STATE_RUNNING);

    // Update package database
    if (!updatePackageDatabase(packages))
        goto error;

    // Status update
    Status::setRepoStateChanged(branchName, name, architecture, "waiting for other processes", "", Status::STATE_WAITING);

    // Trigger signal
    waitingCommit = true;
    emit threadWaiting(this, threadSessionID);

    // Sleep
    mutexWaitCondition.lock();
    if (!waitCondition.wait(&mutexWaitCondition)) {
        threadErrorString = "error: wait condition failed!";
        goto error;
    }

    isCommitting = true;
    waitingCommit = false;


    // Status update
    Status::setRepoStateChanged(branchName, name, architecture, "committing changes", "", Status::STATE_RUNNING);

    // Commit all changes to final destination
    // First create updated symlinks
    if (!applySymlinks(packages, path, "../../.."))
        goto error;

    // Remove old database
    if (QFile::exists(path + "/" + repoDB) && !QFile::remove(path + "/" + repoDB)) {
        threadErrorString = "error: failed to remove old database!";
        goto error;
    }

    // Copy database
    if (!QFile::exists(tmpPath + "/" + repoDB) || !QFile::copy(tmpPath + "/" + repoDB, path + "/" + repoDB)) {
        threadErrorString = "error: failed to copy new database!";
        goto error;
    }

    // Fix file permissions of database
    Global::fixFilePermission(path + "/" + repoDB);

    // Link database
    if (!symlinkExists(path + "/" + repoDBLink) && !QFile::link(repoDB, path + "/" + repoDBLink)) {
        threadErrorString = "error: failed to link new database!";
        goto error;
    }

    // Lock mutex
    mutexUpdatingRepoAttributes.lock();

    // Update repository state
    if (!setNewRandomState()) {
        mutexUpdatingRepoAttributes.unlock();
        threadErrorString = "error: failed to update repository state!";
        goto error;
    }

    // Update packages lists
    overlayPackages = tmpOverlayPackages;
    syncPackages = tmpSyncPackages;
    tmpOverlayPackages.clear();
    tmpSyncPackages.clear();

    // Unlock mutex
    mutexUpdatingRepoAttributes.unlock();

    if (!writePackagesConfig(".overlaypackages", overlayPackages)) {
        threadErrorString = "error: failed to save overlay packages!";
        goto error;
    }

    if (!writePackagesConfig(".syncpackages", syncPackages)) {
        threadErrorString = "error: failed to save sync packages!";
        goto error;
    }

    // Set new branch state
    emit requestNewBranchState();

    // Status update
    Status::setRepoStateChanged(branchName, name, architecture, "finished package commit", "", Status::STATE_SUCCESS);

    // Send e-mail
    Status::setRepoCommit(threadUsername, branchName, name, architecture, addPackages, removePackages);

    emit threadFinished(this, threadSessionID);
    mutexWaitCondition.unlock();
    isCommitting = false;
    threadSessionID = -1;
    threadUsername.clear();
    return;

error:
    // Status update
    Status::setRepoStateChanged(branchName, name, architecture, "process failed", threadErrorString, Status::STATE_FAILED);

    mutexWaitCondition.unlock();
    waitingCommit = false;
    isCommitting = false;
    emit threadFailed(this, threadSessionID);
    threadSessionID = -1;
    threadUsername.clear();
}



bool Repo::cleanupTmpDir() {
    QDir dir(tmpPath);

    if (dir.exists() && !Global::rmDir(tmpPath))
        return false;

    return dir.mkpath(tmpPath);
}



bool Repo::readConfig() {
    // Reset values first
    isSyncRepo = false;

    // Read config
    QFile file(path + "/" + BOXIT_DB_CONFIG);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().split("#", QString::KeepEmptyParts).first().trimmed();
        if (line.isEmpty() || !line.contains("="))
            continue;

        QString arg1 = line.split("=").first().toLower().trimmed();
        QString arg2 = line.split("=").last().trimmed();

        if (arg1 == "state")
            state = arg2;
        else if (arg1 == "sync")
            isSyncRepo = (bool)arg2.toInt();
    }

    file.close();

    return true;
}


bool Repo::updateConfig() {
    QFile file(path + "/" + BOXIT_DB_CONFIG);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "###\n### BoxIt Repository Config\n###\n";
    out << "\nstate=" << state;
    out << "\nsync=" << QString::number((int)isSyncRepo);

    file.close();
    return true;
}



bool Repo::readPackagesConfig(const QString fileName, QStringList & packages) {
    packages.clear();

    QFile file(path + "/" + fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        packages.append(line);
    }

    file.close();

    return true;
}



bool Repo::writePackagesConfig(const QString fileName, const QStringList & packages) {
    QFile file(path + "/" + fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    foreach (const QString package, packages)
        out << package << "\n";

    file.close();

    return true;
}



bool Repo::applySymlinks(const QList<Package> & packages, const QString path, const QString rootLink) {
    // List of local files in path
    QStringList localFiles = QDir(path).entryList(QDir::Files | QDir::System | QDir::NoDotAndDotDot, QDir::Name);

    // Remove all files and symlinks
    foreach (const QString file, localFiles) {
        // Skip database and database link
        if (file == repoDB || file == repoDBLink)
            continue;

        QString dest = path + "/" + file;
        QFileInfo info(dest);

        if (info.isFile() && !QFile::remove(dest)) {
            threadErrorString = "error: failed to remove file '" + dest + "'!";
            return false;
        }
        else if (info.isSymLink() && unlink(dest.toStdString().c_str()) != 0) {
            threadErrorString = "error: failed to remove symlink '" + dest + "'!";
            return false;
        }
    }


    // Create new symlinks
    for (int i = 0; i < packages.size(); ++i) {
        const Package *package = &packages.at(i);

        QString link = rootLink + "/" + package->link;
        QString dest = path + "/" + package->file;

        // Symlink package
        if (!QFile::link(link, dest)) {
            threadErrorString = "error: failed to symlink '" + link + "' to '" + dest + "'!";
            return false;
        }

        link += BOXIT_SIGNATURE_ENDING;
        dest += BOXIT_SIGNATURE_ENDING;

        // Symlink signature
        if (!QFile::link(link, dest)) {
            threadErrorString = "error: failed to symlink '" + link + "' to '" + dest + "'!";
            return false;
        }
    }

    return true;
}



bool Repo::symlinkExists(const QString path) {
    struct stat info;

    return (lstat(path.toStdString().c_str(), &info) == 0);
}



bool Repo::updatePackageDatabase(const QList<Package> & packages) {
    QString dbFile = tmpPath + "/" + repoDB;
    QStringList packagesToRemove, packagesToAdd;
    QList<Package> dbPackages;

    if (QFile::exists(dbFile)) {
        // Create temporary directory
        QString dbTmpPath = tmpPath + "/.boxit_database_extracted";

        if (QDir(dbTmpPath).exists() && !Global::rmDir(dbTmpPath)) {
            threadErrorString = "error: failed to remove temporary database path!";
            return false;
        }

        if (!QDir(dbTmpPath).mkpath(dbTmpPath)) {
            threadErrorString = "error: failed to create temporary database path!";
            return false;
        }

        // Unpack the database file
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.setWorkingDirectory(tmpPath);
        process.start("tar", QStringList() << "-xf" << dbFile << "-C" << dbTmpPath);

        if (!process.waitForFinished(60000)) {
            threadErrorString = "error: archive extract process timeout!";
            return false;
        }

        if (process.exitCode() != 0) {
            threadErrorString = "error: archive extract process failed: " + QString::fromUtf8(process.readAll());
            return false;
        }


        // Get packages list
        QStringList tmpList = QDir(dbTmpPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

        // Cleanup
        Global::rmDir(dbTmpPath);


        // Get packages to remove
        foreach (const QString package, tmpList) {
            Package pkg;
            pkg.name = Global::getNameofPKG(package + "-"); // Add '-' because arch is missing -> getNameofPKG fix
            pkg.version = Global::getVersionofPKG(package + "-");
            dbPackages.append(pkg);

            bool found = false;

            for (int i = 0; i < packages.size(); ++i) {
                const Package *package = &packages.at(i);

                if (pkg.name == package->name && pkg.version == package->version) {
                    // Check if this package exists in the sync and overlay pool -> remove and readd it to the database to be sure, that the right checksum is in the db...
                    if (package->isOverlayPackage && tmpSyncPackages.contains(package->file)) {
                        packagesToAdd.append(package->file);
                        break;
                    }

                    found = true;
                    break;
                }
            }

            if (!found)
                packagesToRemove.append(pkg.name);
        }
    }

    // Get package to add
    for (int i = 0; i < packages.size(); ++i) {
        const Package *package = &packages.at(i);

        bool found = false;

        for (int i = 0; i < dbPackages.size(); ++i) {
            const Package *dbPackage = &dbPackages.at(i);

            if (package->name == dbPackage->name && package->version == dbPackage->version) {
                found = true;
                break;
            }
        }

        if (!found)
            packagesToAdd.append(package->file);
    }


    // Remove old packages from database
    if (!removePackagesFromDatabase(packagesToRemove))
        return false;

    // Add new packages to database
    if (!addPackagesToDatabase(packagesToAdd))
        return false;

    return true;
}



bool Repo::addPackagesToDatabase(const QStringList & packages) {
    if (packages.isEmpty())
        return true;

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setWorkingDirectory(tmpPath);
    process.start("repo-add", QStringList() << repoDB <<  packages);

    if (!process.waitForFinished(-1)) {
        threadErrorString = "error: add packages to database process failed to finish!";
        return false;
    }

    if (process.exitCode() != 0) {
        threadErrorString = "error: failed to add packages to database: " + QString::fromUtf8(process.readAll());
        return false;
    }

    return true;
}



bool Repo::removePackagesFromDatabase(const QStringList & packages) {
    if (packages.isEmpty())
        return true;

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setWorkingDirectory(tmpPath);
    process.start("repo-remove", QStringList() << repoDB <<  packages);

    if (!process.waitForFinished(-1)) {
        threadErrorString = "error: remove packages from database process failed to finish!";
        return false;
    }

    if (process.exitCode() != 0) {
        threadErrorString = "error: failed to remove packages from database: " + QString::fromUtf8(process.readAll());
        return false;
    }

    return true;
}
