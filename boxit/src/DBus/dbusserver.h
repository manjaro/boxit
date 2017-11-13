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

#ifndef DBUSSERVER_H
#define DBUSSERVER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDBusAbstractAdaptor>
#include <QDBusError>
#include <QDateTime>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>

#include "simplecrypt.h"



class DBusServer : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.BoxIt-32.Keyring")

public:
    explicit DBusServer(QObject *parent = 0);

public slots:
    bool setHost(const QString uniqueBuildHash, const QString host);
    bool setUserLogin(const QString uniqueBuildHash, const QString username, const QString passwordHash);

    QString getHost(const QString uniqueBuildHash);
    QStringList getUserLogin(const QString uniqueBuildHash);

private:
    QTimer timer;
    SimpleCrypt crypto;
    QMutex mutex;
    QString encryptedUsername, encryptedPassword, encryptedHost;

private slots:
    void timeout();

};

#endif // DBUSSERVER_H
