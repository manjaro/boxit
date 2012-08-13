/*
 *  Fuchs - Manjaro Repository Management
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

#include "userdbs.h"

QMutex UserDatabase::mutex;



bool UserDatabase::loginUser(QString username, QString password, User &user) {
    QMutexLocker locker(&mutex);

    username = username.trimmed().toLower();

    // Check if Username and Password are right
    QList<User> users = _getAllUsersData();

    for (int i = 0; i < users.size(); ++i) {
        if (username != users[i].getUsername() || !users[i].comparePassword(password))
            continue;

        user = users[i];
        user.setAuthorized(true);
        return true;
    }

    return false;
}



bool UserDatabase::setUserData(User &user, bool remove) {
    QMutexLocker locker(&mutex);
    QList<User> users = _getAllUsersData();
    users.removeAll(user);

    if (!remove)
        users.append(user);


    QFile file(BOXIT_USER_DBFILE);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        // TODO: Log Message!
        return false;
    }

    QXmlStreamWriter stream(&file);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();
    stream.writeStartElement("userdb");

    for (int i = 0; i < users.size(); ++i) {
        stream.writeStartElement("user");

        stream.writeTextElement("username", users[i].getUsername());
        stream.writeTextElement("password", users[i].getEnPassword());

        stream.writeEndElement(); // user
    }

    stream.writeEndElement(); // userdb
    stream.writeEndDocument();


    file.close();

    return true;
}




//###
//### Private
//###



QList<User> UserDatabase::_getAllUsersData() {
    QList<User> Users;

    QFile file(BOXIT_USER_DBFILE);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        // TODO: Log Message!
        return Users;
    }

    QXmlStreamReader reader(&file);

    if (!reader.readNextStartElement()) {
         // TODO: Log Message
        file.close();
        return Users;
    }


    if (reader.name() == "userdb") {
        while (reader.readNextStartElement()) {
            if (reader.name() == "user") {
                User user;
                readXMLUserElements(reader, user);

                if (!user.getUsername().isEmpty() && !user.getEnPassword().isEmpty()) {
                    Users.append(user);
                }
            }
            else {
                // TODO: Log Message
                reader.skipCurrentElement();
            }
        }
    }
    else {
        // TODO: Log Message
    }


    // TODO: Log Message!
    if (reader.hasError()) {
       //reader.errorString()
    } else if (file.error() != QFile::NoError) {
        //file.errorString()
    }

    file.close();

    return Users;
}



void UserDatabase::readXMLUserElements(QXmlStreamReader &reader, User &user) {
    while (reader.readNextStartElement()) {
        if (reader.name() == "username") {
            user.setUsername(reader.readElementText().toLower().trimmed());
        }
        else if (reader.name() == "password") {
            user.setEnPassword(reader.readElementText());
        }
        else {
            reader.skipCurrentElement();
        }
    }
}
