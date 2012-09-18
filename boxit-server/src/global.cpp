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




//###
//### Static methods
//###


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



bool Global::fixFilePermission(QString file) {
    int ret;

    // Set right permission
    mode_t process_mask = umask(0);
    ret = chmod(file.toAscii().data(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH);
    umask(process_mask);

    return (ret == 0);
}



bool Global::sendMemoEMail(QString username, QString repository, QString architecture, QStringList addedFiles, QStringList removedFiles) {
    // Prepare e-mail message
    removedFiles.sort();
    addedFiles.sort();

    QString mailMessage = "### BoxIt memo ###\n\n";
    mailMessage += QString("User %1").arg(username);

    if (!addedFiles.isEmpty())
        mailMessage += QString(" committed %1 new package(s)").arg(QString::number(addedFiles.size()));

    if (!addedFiles.isEmpty() && !removedFiles.isEmpty())
        mailMessage += " and";

    if (!removedFiles.isEmpty())
        mailMessage += QString(" removed %1 package(s) from repository %2 %3").arg(QString::number(removedFiles.size()), repository, architecture);
    else
        mailMessage += QString(" to repository %1 %2").arg(repository, architecture);

    mailMessage += ".";

    if (!addedFiles.isEmpty())
        mailMessage += "\n\n\n### New package(s) ###\n\n" + addedFiles.join("\n");

    if (!removedFiles.isEmpty())
        mailMessage += "\n\n\n### Removed package(s) ###\n\n" + removedFiles.join("\n");

    // Send e-mail message to mailing lists
    bool ret = true;

    for (int i = 0; i < config.mailingListEMails.size(); ++i) {
        if (!Global::sendEMail("[BoxIt] commit memo", config.mailingListEMails.at(i), mailMessage))
            ret = false;
    }

    return ret;
}



bool Global::sendEMail(QString subject, QString to, QString text) {
    QProcess process;
    process.start("mail", QStringList() << "-s" << subject << to);
    if (!process.waitForStarted())
        return false;

    QStringList lines = text.split("\n", QString::KeepEmptyParts);
    for (int i = 0; i < lines.size(); ++i) {
        process.write(QString(lines.at(i) + "\n").toAscii());
    }

    process.closeWriteChannel();

    if (!process.waitForFinished())
        return false;
    if (process.exitCode() != 0)
        return false;

    return true;
}



bool Global::rmDir(QString path, bool onlyHidden, bool onlyContent) {
    if (!QDir().exists(path))
        return true;

    bool success = true;

    // Remove content of dir
    QStringList list = QDir(path).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);

    for (int i = 0; i < list.size(); ++i) {
        if (onlyHidden && !list.at(i).trimmed().startsWith("."))
            continue;

        QFileInfo info(path + "/" + list.at(i));

        if (info.isDir()) {
            if (!rmDir(path + "/" + list.at(i)), onlyHidden)
                success = false;
        }
        else {
            if (!QFile::remove(path + "/" + list.at(i)))
                success = false;
        }
    }

    if (!onlyContent && !QDir().rmdir(path))
        return false;

    return success;
}



bool Global::copyDir(QString src, QString dst, bool hidden) {
    if (!QDir().exists(src))
        return false;
    if (QDir().exists(dst))
        rmDir(dst);
    if (!QDir().mkpath(dst))
        return false;

    bool success = true;

    // Copy content of dir
    QStringList list;

    if (hidden)
        list = QDir(src).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden, QDir::Name);
    else
        list = QDir(src).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System, QDir::Name);


    for (int i = 0; i < list.size(); ++i) {
        QFileInfo info(src + "/" + list.at(i));

        if (info.isDir()) {
            if (!copyDir(src + "/" + list.at(i), dst + "/" + list.at(i)))
                success = false;
        }
        else {
            if (!QFile::copy(src + "/" + list.at(i), dst + "/" + list.at(i)))
                success = false;
        }
    }

    return success;
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
    config.poolDir.clear();
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

        QString arg1 = line.split("=").first().toLower().trimmed(), arg2 = line.split("=").last().trimmed();

        if (arg1 == "salt") {
            config.salt = arg2;
        }
        else if (arg1 == "repodir") {
            config.repoDir = arg2;
            config.poolDir = arg2 + "/" + BOXIT_POOL_REPO;
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
