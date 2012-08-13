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

#include "user.h"

User::User() {
    clear();
}



bool User::User::operator==(User compare)
{
    return (username == compare.username);
}



bool User::comparePassword(QString checkPwd) {
    return compareSaltHash(checkPwd, enPassword);
}



bool User::setPassword(QString password) {
    if (password.isEmpty())
        return false;

    enPassword = encryptSaltHash(password);
    return true;
}



void User::clear() {
    authorized = false;
    username.clear();
    enPassword.clear();
}



//###
//### Private
//###



QString User::encryptSaltHash(QString str) {
    return QString(QCryptographicHash::hash(QString(QString(Global::getConfig().salt) + str).toLocal8Bit(), QCryptographicHash::Sha1).toHex());
}



bool User::compareSaltHash(QString str, QString hash) {
    return (encryptSaltHash(str) == hash);
}

