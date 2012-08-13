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

#ifndef USER_H
#define USER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QCryptographicHash>
#include "const.h"
#include "global.h"


class User {
public:
    User();
    bool operator==(User compare);

    QString getUsername() { return username; }
    QString getEnPassword() { return enPassword; }
    bool isAuthorized() { return authorized; }

    void setUsername(QString username) { User::username = username; }
    void setEnPassword(QString enPassword) { User::enPassword = enPassword; }
    void setAuthorized(bool authorized) { User::authorized = authorized; }

    bool setPassword(QString password);
    bool comparePassword(QString checkPwd);
    void clear();

private:
    bool authorized;
    QString username, enPassword;

    QString encryptSaltHash(QString str);
    bool compareSaltHash(QString str, QString hash);
};


#endif // USER_H
