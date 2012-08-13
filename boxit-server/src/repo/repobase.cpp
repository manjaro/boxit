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

#include "repobase.h"



RepoBase::RepoBase(QString name, QString path, QString architecture) :
    name(name),
    path(path),
    workPath(QString(BOXIT_TMP) + "/" + name + "/" + architecture),
    architecture(architecture),
    repoDB(name + QString(BOXIT_DB_ENDING)),
    repoDBLink(name + QString(BOXIT_DB_LINK_ENDING))
{
    locked = false;
    tmpSyncAvailable = false;
    lockedUsername.clear();
    threadState = "-";
    threadJob = "-";
    syncURL = "";
    syncExcludeFile = ".sync_exclude";

    // If the sync exclude file doesn't exists, then create it...
    QFile file(path + "/" + syncExcludeFile);
    if (!file.exists() && file.open(QIODevice::WriteOnly | QIODevice::Text))
        file.close();
}



RepoBase::~RepoBase() {
    // Cleanup
    cleanupTMPDir();
}



bool RepoBase::operator==(const RepoBase &repo) {
    return (name == repo.name && architecture == repo.architecture);
}



bool RepoBase::update() {
    // Cleanup first
    packages.clear();
    signatures.clear();
    removePackages.clear();
    tmpSyncAvailable = false;

    // Get all packages
    packages = QDir(path).entryList(QString(BOXIT_FILE_FILTERS).split(" ", QString::SkipEmptyParts), QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    packages.removeDuplicates();

    // Get all signature files
    signatures = QDir(path).entryList(QStringList() << "*" + QString(BOXIT_SIGNATURE_ENDING), QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    signatures.removeDuplicates();

    // Read config
    if (!readConfig())
        return false;

    // Prepare work dir
    if (!cleanupTMPDir(true))
        return false;

    return true;
}



bool RepoBase::cleanupTMPDir(bool recreate) {
    QString tmpDir = QString(BOXIT_TMP) + "/" + name + "/" + architecture;

    if (QDir(tmpDir).exists() && !Global::rmDir(tmpDir))
        return false;

    if (recreate && !QDir().mkpath(tmpDir))
        return false;

    return true;
}



void RepoBase::lock(QString username) {  
    lockedUsername = username;
    locked = true;

    update();
}



void RepoBase::unlock() {
    lockedUsername.clear();
    locked = false;
}



void RepoBase::updateName(QString name) {
    this->name = name;
    workPath = QString(BOXIT_TMP) + "/" + name + "/" + architecture;
    repoDB = name + QString(BOXIT_DB_ENDING);
    repoDBLink = name + QString(BOXIT_DB_LINK_ENDING);
}




bool RepoBase::setSyncURL(QString url) {
    if (url == "!")
        syncURL.clear();
    else
        syncURL = url;

    return updateConfig();
}



bool RepoBase::setNewRandomState() {
    // Create new state
    state = QString(QCryptographicHash::hash(QString(QDateTime::currentDateTime().toString(Qt::ISODate) + QString::number(qrand())).toLocal8Bit(), QCryptographicHash::Sha1).toHex());
    return updateConfig();
}



void RepoBase::removeFile(QString filename) {
    if (QFile::exists(path + "/" + filename))
        removePackages.append(filename);
}




//###
//### Private methods
//###



bool RepoBase::readConfig() {
    QFile file(path + "/" + BOXIT_REPO_CONFIG);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().split("#", QString::KeepEmptyParts).first().trimmed();
        if (line.isEmpty() || !line.contains("="))
            continue;

        QString arg1 = line.split("=").first().toLower().trimmed(), arg2 = line.split("=").last().trimmed();

        if (arg1 == "state")
            state = arg2;
        else if (arg1 == "sync")
            syncURL = arg2;
    }
    file.close();

    return true;
}



bool RepoBase::updateConfig() {
    QFile file(path + "/" + BOXIT_REPO_CONFIG);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "###\n### BoxIt Repository Config\n###\n";
    out << "\nstate = " << state;
    out << "\nsync = " << syncURL;

    file.close();
    return true;
}
