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

#include "dbusclient.h"



DBusClient::DBusClient() :
    QDBusInterface("org.BoxIt32.Keyring", "/", "org.BoxIt32.Keyring", QDBusConnection::sessionBus())
{
}



bool DBusClient::setHost(const QString host) {
    if (!isValid())
        return false;

    QDBusReply<bool> reply = call("setHost", QString(UNIQUE_BUILD_HASH), host);
    if (!reply.isValid())
        return false;

    return reply.value();
}



bool DBusClient::setUserLogin(const QString username, const QString passwordHash) {
    if (!isValid())
        return false;

    QDBusReply<bool> reply = call("setUserLogin", QString(UNIQUE_BUILD_HASH), username, passwordHash);
    if (!reply.isValid())
        return false;

    return reply.value();
}



bool DBusClient::getHost(QString & host) {
    if (!isValid())
        return false;

    QDBusReply<QString> reply = call("getHost", QString(UNIQUE_BUILD_HASH));
    if (!reply.isValid())
        return false;

    host = reply.value();

    return !host.isEmpty();
}



bool DBusClient::getUserLogin(QString & username, QString & passwordHash) {
    if (!isValid())
        return false;

    QDBusReply<QStringList> reply = call("getUserLogin", QString(UNIQUE_BUILD_HASH));
    if (!reply.isValid())
        return false;

    QStringList data = reply.value();
    if (data.size() < 2)
        return false;

    username = data.at(0);
    passwordHash = data.at(1);

    return true;
}
