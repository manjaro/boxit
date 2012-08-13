/*
 *  Manjaro Repository Management
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

#ifndef USERDBS_H
#define USERDBS_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QMutex>
#include <QMutexLocker>
#include "const.h"
#include "user.h"


class UserDatabase
{
public:
    static bool loginUser(QString username, QString password, User &user);
    static bool setUserData(User &user, bool remove = false);
    static QList<User> getAllUsersData() { QMutexLocker locker(&mutex); return _getAllUsersData(); }

private:
    static QMutex mutex;

    static QList<User> _getAllUsersData();
    static void readXMLUserElements(QXmlStreamReader &reader, User &user);

};

#endif // USERDBS_H
