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



Repo::Repo(QString name, QString path, QString architecture) :
    name(name),
    path(path),
    architecture(architecture),
    repoDB(name + QString(BOXIT_DB_ENDING)),
    repoDBLink(name + QString(BOXIT_DB_LINK_ENDING))
{
    syncURL = "";
    syncExcludeFile = ".sync_exclude";

    // If the sync exclude file doesn't exists, then create it...
    QFile file(path + "/" + syncExcludeFile);
    if (!file.exists() && file.open(QIODevice::WriteOnly | QIODevice::Text))
        file.close();
}



bool Repo::operator==(const Repo &repo) {
    return (name == repo.name && architecture == repo.architecture);
}



bool Repo::update() {
    // Cleanup first
    packages.clear();
    signatures.clear();

    // Get all packages
    packages = QDir(path).entryList(QString(BOXIT_FILE_FILTERS).split(" ", QString::SkipEmptyParts), QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    packages.removeDuplicates();

    // Get all signature files
    signatures = QDir(path).entryList(QStringList() << "*" + QString(BOXIT_SIGNATURE_ENDING), QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    signatures.removeDuplicates();

    // Read config
    if (!readConfig())
        return false;

    return true;
}




void Repo::updateName(QString name) {
    this->name = name;
    repoDB = name + QString(BOXIT_DB_ENDING);
    repoDBLink = name + QString(BOXIT_DB_LINK_ENDING);
}




bool Repo::setSyncURL(QString url) {
    if (url == "!")
        syncURL.clear();
    else
        syncURL = url;

    return updateConfig();
}



bool Repo::setNewRandomState() {
    // Create new state
    state = QString(QCryptographicHash::hash(QString(QDateTime::currentDateTime().toString(Qt::ISODate) + QString::number(qrand())).toLocal8Bit(), QCryptographicHash::Sha1).toHex());
    return updateConfig();
}




//###
//### Private methods
//###



bool Repo::readConfig() {
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



bool Repo::updateConfig() {
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
