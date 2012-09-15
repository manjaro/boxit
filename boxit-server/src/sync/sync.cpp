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

Sync::Sync(const QString destPath, QObject *parent) :
    QObject(parent),
    destPath(destPath),
    download(destPath, this)
{
    busy = false;
}



Sync::~Sync() {
    download.cancle();
}



bool Sync::synchronize(QString url, const QString repoName, const QString excludeFilePath, QStringList &allDBPackages, QStringList &addedFiles, QStringList checkFilePaths, QStringList onlyFiles) {
    QStringList patterns;
    QList<Package> downloadPackages;

    if (busy) {
        emit error("A synchronization process is already running!");
        return false;
    }

    busy = true;

    // Init
    if (url.isEmpty() || !QDir(destPath).exists())
        goto error;

    if (url.mid(url.length() - 1, 1) != "/")
        url += "/";


    // First download the database...
    if (!downloadFile(url + repoName + BOXIT_DB_ENDING))
        goto error; // Error messages are emited by the method...

    // Fill the packages list
    if (!fillPackagesList(repoName))
        goto error; // Error messages are emited by the method...

    // Remove database file again
    QFile::remove(destPath + "/" + repoName + BOXIT_DB_ENDING); // Error isn't important


    // Add our destination path to the check list to check if the file already exist
    checkFilePaths.append(destPath);
    checkFilePaths.removeDuplicates();


    // Check if packages in onlyFiles list exist in the database
    if (!onlyFiles.isEmpty()) {
        for (int x = 0; x < onlyFiles.size(); ++x) {
            QString currentPackage = onlyFiles.at(x);
            bool found = false;

            for (int i = 0; i < packages.size(); ++i) {
                if (packages.at(i).packageName == currentPackage) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                emit error(QString("Selected package '%1' doesn't exist in database!").arg(currentPackage));
                goto error;
            }
        }
    }


    // Read exclude file
    if (!readExcludeFile(excludeFilePath, patterns))
        goto error; // Error messages are emited by the method...


    // Create a list only with the packages to download
    for (int i = 0; i < packages.size(); ++i) {
        Package package = packages.at(i);
        allDBPackages.append(package.fileName);

        if (!onlyFiles.isEmpty() && !onlyFiles.contains(package.packageName))
            continue;

        // Check if the file is blacklisted, but only if this isn't excplicit forced by the user
        if (onlyFiles.isEmpty() && matchWithWildcard(package.packageName, patterns))
            continue;

        addedFiles.append(package.fileName);

        // Check if file already exists
        if (fileAlreadyExist(package, checkFilePaths))
            continue;

        downloadPackages.append(package);
    }


    // Download each file
    for (int i = 0; i < downloadPackages.size(); ++i) {
        // Update status
        emit status(i + 1, downloadPackages.size());

        Package package = downloadPackages.at(i);

        // Download file...
        if (package.downloadPackage) {
            if (!downloadFile(url + package.fileName))
                goto error; // Error messages are emited by the method...

            // Check if package checksum is ok
            if (CryptSHA256::sha256CheckSum(destPath + "/" + package.fileName) != package.sha256sum) {
                emit error(QString("Package checksum doesn't match for package '%1'!").arg(package.fileName));
                goto error;
            }
        }

        // Download signature file...
        if (package.downloadSignature && !downloadFile(url + package.fileName + BOXIT_SIGNATURE_ENDING))
            // Error isn't fatal
            emit error(QString("warning: package '%1' doesn't has a signature!").arg(package.fileName));
    }

    busy = false;
    return true;

error:
    busy = false;
    return false;
}



//###
//### Private methods
//###



bool Sync::downloadFile(QString url) {
    QEventLoop eventLoop;
    QObject::connect(&download, SIGNAL(finished(bool)), &eventLoop, SLOT(quit()));

    if (!download.download(url)) {
        emit error("failed to start download!");
        return false;
    }

    eventLoop.exec();

    if (download.isError()) {
        emit error(download.lastError());
        return false;
    }

    return true;
}



bool Sync::fillPackagesList(const QString repoName) {
    QString dbPath = destPath + "/.db";

    // Create working folder
    if ((QDir(dbPath).exists() && !Global::rmDir(dbPath))
            || !QDir().mkpath(dbPath))
        return false;


    // Unpack the database file
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setWorkingDirectory(dbPath);
    process.start("tar", QStringList() << "-xf" << destPath + "/" + repoName + BOXIT_DB_ENDING << "-C" << dbPath);

    if (!process.waitForFinished(60000)) {
        emit error("archive extract process timeout!");
        return false;
    }

    if (process.exitCode() != 0) {
        emit error("archive extract process failed: " + QString(process.readAll()));
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
            emit error(QString("failed to read database file '%1'!").arg(path));
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
            emit error(QString("uncomplete desc file '%1'!").arg(path));
            return false;
        }

        packages.append(package);
    }

    // Cleanup
    Global::rmDir(dbPath); // Error isn't important...

    return true;
}



bool Sync::fileAlreadyExist(Package &package, const QStringList &checkFilePaths) {
    package.downloadPackage = true;
    package.downloadSignature = true;

    // Check if file already exists
    for (int i = 0; i < checkFilePaths.size(); ++i) {
        QString path = checkFilePaths.at(i) + "/" + package.fileName;

        if (QFile::exists(path) && CryptSHA256::sha256CheckSum(path) == package.sha256sum)
            package.downloadPackage = false;

        if (QFile::exists(path + BOXIT_SIGNATURE_ENDING))
            package.downloadSignature = false;
    }

    return (!package.downloadPackage && !package.downloadSignature);
}



bool Sync::readExcludeFile(const QString filePath, QStringList &patterns) {
    QFile file(filePath);

    if (!file.exists()) {
        emit error(QString("warning: exclude file '%1' does not exist!").arg(filePath));
        return true; // This shouldn't produce an error...
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit error(QString("failed to read file '%1'!").arg(filePath));
        return false;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (!line.isEmpty())
            patterns.append(line);
    }

    file.close();
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
