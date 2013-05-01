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

#include "global.h"



//###
//### Static variables
//###

Global::Config Global::config;
int Global::lastSessionID = 10;
QMutex Global::newSessionIDMutex;




//###
//### Static methods
//###


int Global::getNewUniqueSessionID() {
    QMutexLocker locker(&newSessionIDMutex);

    ++lastSessionID;
    return lastSessionID;
}




QString Global::getNameofPKG(QString pkg) {
    QString work;
    pkg = pkg.split("/", QString::SkipEmptyParts).last();
    work = pkg.section("-", -3, -1);

    return QString(pkg).remove(pkg.size() - (work.size() + 1), work.size() + 1).trimmed();
}



QString Global::getVersionofPKG(QString pkg) {
    pkg = pkg.split("/", QString::SkipEmptyParts).last();
    pkg = pkg.section("-", -3, -1);
    pkg.remove(pkg.lastIndexOf("-"), pkg.size());

    return pkg;
}



QByteArray Global::sha1CheckSum(const QString filePath) {
    QCryptographicHash crypto(QCryptographicHash::Sha1);
    QFile file(filePath);

    if (!file.open(QFile::ReadOnly))
        return QByteArray();

    while(!file.atEnd()){
        crypto.addData(file.read(8192));
    }

    return crypto.result();
}



bool Global::sendMemoEMail(const QString mailPrefixMessage, const QList<RepoChanges> & repoChanges) {
    QStringList attachments;
    const QString tmpPath = QString(BOXIT_STATUS_TMP) + "/" + QString::number(qrand()) + "_" + QDateTime::currentDateTime().toString(Qt::ISODate);
    bool success = true;

    // Create working folder
    if (QDir(tmpPath).exists() && !Global::rmDir(tmpPath)) {
        cerr << "error: failed to remove folder '" << tmpPath.toUtf8().data() << "'" << endl;
        return false;
    }

    if (!QDir().mkpath(tmpPath)) {
        cerr << "error: failed to create folder '" << tmpPath.toUtf8().data() << "'" << endl;
        return false;
    }

    QString message = mailPrefixMessage;

    for (int i = 0; i < repoChanges.size(); ++i) {
        const RepoChanges *repo = &repoChanges.at(i);

        message += QString(" - %1 %2 %3:  %4 new and %5 removed package(s)\n").arg(repo->branchName, repo->repoName, repo->repoArchitecture,
                                                                                QString::number(repo->addedPackages.size()),
                                                                                QString::number(repo->removedPackages.size()));

        // Create attachment file
        QFile file(QString(tmpPath) + "/" + repo->branchName + "_" + repo->repoName + "_" + repo->repoArchitecture);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            success = false;
            goto remove_tmp_dir;
        }

        QTextStream out(&file);
        if (!repo->addedPackages.isEmpty()) {
            out << "[New Packages]\n" << repo->addedPackages.join("\n");

            if (!repo->removedPackages.isEmpty())
                out << "\n\n\n";
        }

        if (!repo->removedPackages.isEmpty())
            out << "[Removed Packages]\n" << repo->removedPackages.join("\n");

        file.close();

        // Add to list
        attachments.append(file.fileName());
    }


    // Send e-mail
    if (!Global::sendMemoEMail(message, attachments)) {
        cerr << "error: failed to send e-mail!" << endl;
        success = false;
        goto remove_tmp_dir;
    }


remove_tmp_dir:

    // Remove working folder again
    if (QDir(tmpPath).exists() && !Global::rmDir(tmpPath))
        cerr << "error: failed to remove folder '" << tmpPath.toUtf8().data() << "'" << endl;

    return success;
}



bool Global::sendMemoEMail(const QString mailMessage, const QStringList attachments) {
    // Send e-mail message to mailing lists
    bool ret = true;

    for (int i = 0; i < config.mailingListEMails.size(); ++i) {
        if (!Global::sendEMail("[BoxIt] Memo", config.mailingListEMails.at(i), mailMessage, attachments))
            ret = false;
    }

    return ret;
}



bool Global::sendEMail(const QString subject, const QString to, const QString text, const QStringList attachments) {
    QStringList args;

    foreach (QString attachment, attachments)
        args << "-a" << attachment;

    args << "-s" << subject << to;

    QProcess process;
    process.start("mail", args);
    if (!process.waitForStarted())
        return false;

    QStringList lines = text.split("\n", QString::KeepEmptyParts);
    for (int i = 0; i < lines.size(); ++i) {
        process.write(QString(lines.at(i) + "\n").toUtf8());
    }

    process.closeWriteChannel();

    if (!process.waitForFinished())
        return false;
    if (process.exitCode() != 0)
        return false;

    return true;
}



bool Global::rmDir(const QString path, const bool onlyHidden, const bool onlyContent) {
    if (!QDir().exists(path))
        return true;

    bool success = true;

    // Remove content of dir
    QStringList list = QDir(path).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);

    foreach (const QString file, list) {
        if (onlyHidden && !file.trimmed().startsWith("."))
            continue;

        QFileInfo info(path + "/" + file);

        if (info.isDir()) {
            if (!rmDir(path + "/" + file), onlyHidden)
                success = false;
        }
        else {
            if (!QFile::remove(path + "/" + file))
                success = false;
        }
    }

    if (!onlyContent && !QDir().rmdir(path))
        return false;

    return success;
}



bool Global::copyDir(const QString src, const QString dst, const bool hidden) {
    if (!QDir().exists(src))
        return false;

    if (QDir().exists(dst))
        rmDir(dst);

    if (!QDir().mkpath(dst))
        return false;

    // Keep same folder permission
    preserveDirectoryPermission(src, dst); // Error isn't critical


    bool success = true;

    // Copy content of dir
    QStringList list;

    if (hidden)
        list = QDir(src).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden, QDir::Name);
    else
        list = QDir(src).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System, QDir::Name);


    foreach (const QString file, list) {
        QFileInfo info(src + "/" + file);

        if (info.isDir()) {
            if (!copyDir(src + "/" + file, dst + "/" + file, hidden))
                success = false;
        }
        else if (info.isSymLink()) {
            QString symlinkTarget = getSymlinkTarget(src + "/" + file);

            if (symlinkTarget.isEmpty() || !QFile::link(symlinkTarget, dst + "/" + file))
                success = false;
        }
        else {
            if (!QFile::copy(src + "/" + file, dst + "/" + file))
                success = false;
        }
    }

    return success;
}



QString Global::getSymlinkTarget(const QString symlink) {
    char buf[1024];
    ssize_t len;

    if ((len = readlink(symlink.toStdString().c_str(), buf, sizeof(buf) - 1)) != -1) {
        buf[len] = '\0';
        return QString::fromLatin1(buf);
    }

    return "";
}



bool Global::fixFilePermission(const QString file) {
    return setFilePermission(file, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH);
}



bool Global::setFilePermission(const QString file, const mode_t mode) {
    int ret;

    // Set right permission
    mode_t process_mask = umask(0);
    ret = chmod(file.toStdString().c_str(), mode);
    umask(process_mask);

    return (ret == 0);
}



bool Global::preserveDirectoryPermission(const QString srcDir, const QString destDir) {
    if (!QDir(srcDir).exists())
        return false;

    // Get source folder permission
    struct stat srcStat;
    stat(srcDir.toStdString().c_str(), &srcStat);

    // Set destination folder permission
    return setFilePermission(destDir, srcStat.st_mode);
}



//###
//### Config
//###



Global::Config Global::getConfig() {
    return config;
}



bool Global::readConfig() {
    // Cleanup
    config.salt.clear();
    config.repoDir.clear();
    config.syncPoolDir.clear();
    config.overlayPoolDir.clear();
    config.sslCertificate.clear();
    config.sslKey.clear();
    config.mailingListEMails.clear();

    // Read config
    QFile file(BOXIT_SERVER_CONFIG);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().split("#", QString::KeepEmptyParts).first().trimmed();
        if (line.isEmpty() || !line.contains("="))
            continue;

        QString arg1 = line.split("=").first().toLower().trimmed();
        QString arg2 = line.split("=").last().trimmed();

        if (arg1 == "salt") {
            config.salt = arg2;
        }
        else if (arg1 == "repodir") {
            config.repoDir = arg2;
            config.syncPoolDir = arg2 + "/" + BOXIT_SYNC_POOL;
            config.overlayPoolDir = arg2 + "/" + BOXIT_OVERLAY_POOL;
        }
        else if (arg1 == "sslcertificate") {
            config.sslCertificate = arg2;
        }
        else if (arg1 == "sslkey") {
            config.sslKey = arg2;
        }
        else if (arg1 == "mailinglistemail") {
            config.mailingListEMails.append(arg2);
        }
    }
    file.close();

    if (config.salt.isEmpty() || config.repoDir.isEmpty() || config.sslCertificate.isEmpty() || config.sslKey.isEmpty() || config.mailingListEMails.isEmpty())
        return false;

    return true;
}
