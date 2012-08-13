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

#include "version.h"

Version::Version(QString string) :
    QString(string)
{
}



bool Version::operator<(Version const& version) const {
    if (compareVersions(*this, version) == 0)
        return true;

    return false;
}



bool Version::operator>(Version const& version) const {
    if (compareVersions(*this, version) == 1)
        return true;

    return false;
}



bool Version::operator==(Version const& version) const {
    if (compareVersions(*this, version) == -1)
        return true;

    return false;
}



bool Version::isValid() {
    return (*this != "-1");
}




// Private



int Version::compareVersions(Version version1, Version version2) const {
    QList<int> versionlist1 = splittoIntList(Version(version1.split("-").at(0).trimmed()));
    QList<int> versionlist2 = splittoIntList(Version(version2.split("-").at(0).trimmed()));
    int size;

    if (versionlist1.size() < versionlist2.size())
        size = versionlist2.size();
    else
        size = versionlist1.size();


    for (int i = 0; i < size; ++i) {
        int ver1 = 0, ver2 = 0;

        if (i < versionlist1.size())
            ver1 = versionlist1.at(i);

        if (i < versionlist2.size())
            ver2 = versionlist2.at(i);

        if (ver1 < ver2)
            return 0;
        else if (ver1 > ver2)
            return 1;
    }

    if (version1.contains("-") || version2.contains("-"))
        return compareVersions(Version(QString(version1).remove(0, 1 + version1.split("-").at(0).trimmed().size()).trimmed()), Version(QString(version2).remove(0, 1 + version2.split("-").at(0).trimmed().size()).trimmed()));

    return -1;
}



QList<int> Version::splittoIntList(Version version) const {
    QString work;
    QList<int> list;

    for (int i = 0; i < version.size(); ++i) {
        if (!version.at(i).isNumber()) {
            if (!work.isEmpty())
                list.append(work.toInt());

            work.clear();
            continue;
        }

        work += version.at(i);
    }

    if (!work.isEmpty())
        list.append(work.toInt());

    return list;
}
