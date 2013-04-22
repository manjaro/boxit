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

#include "dbusserver.h"



DBusServer::DBusServer(QObject *parent) :
    QDBusAbstractAdaptor(parent),
    crypto(0)
{
    setAutoRelaySignals(true);

    qsrand(uint(QDateTime::currentMSecsSinceEpoch() & 0xFFFF));
    crypto.setKey(qrand());

    timer.setInterval(900000); // 15 minutes
    connect(&timer, SIGNAL(timeout())   ,   this, SLOT(timeout()));
    timer.start();
}



bool DBusServer::setHost(const QString uniqueBuildHash, const QString host) {
    QMutexLocker locker(&mutex);

    if (uniqueBuildHash != UNIQUE_BUILD_HASH)
        return false;

    // Restart timer
    timer.stop();
    timer.start();

    encryptedHost = crypto.encryptToString(host);

    return true;
}



bool DBusServer::setUserLogin(const QString uniqueBuildHash, const QString username, const QString passwordHash) {
    QMutexLocker locker(&mutex);

    if (uniqueBuildHash != UNIQUE_BUILD_HASH)
        return false;

    // Restart timer
    timer.stop();
    timer.start();

    encryptedUsername = crypto.encryptToString(username);
    encryptedPassword = crypto.encryptToString(passwordHash);

    return true;
}



QString DBusServer::getHost(const QString uniqueBuildHash) {
    QMutexLocker locker(&mutex);

    if (uniqueBuildHash != UNIQUE_BUILD_HASH || encryptedHost.isEmpty())
        return "";

    return crypto.decryptToString(encryptedHost);
}



QStringList DBusServer::getUserLogin(const QString uniqueBuildHash) {
    QMutexLocker locker(&mutex);

    QStringList list;

    if (uniqueBuildHash != UNIQUE_BUILD_HASH || encryptedUsername.isEmpty() || encryptedPassword.isEmpty())
        return list;

    list << crypto.decryptToString(encryptedUsername);
    list << crypto.decryptToString(encryptedPassword);

    return list;
}



void DBusServer::timeout() {
    QMutexLocker locker(&mutex);

    encryptedHost.clear();
    encryptedUsername.clear();
    encryptedPassword.clear();
}
