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

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <dbusserver.h>
#include <dbusclient.h>
#include <iostream>

using namespace std;


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Check if another server is already running
    DBusClient *dbusClient = new DBusClient;

    if (dbusClient->isValid()) {
        delete dbusClient;
        cerr << "error: another BoxIt session DBus is already running!" << endl;
        return 1;
    }

    delete dbusClient;


    // Start DBus server
    new DBusServer(&app);

    QDBusConnection connection = QDBusConnection::sessionBus();

    if (!connection.isConnected())
    {
        cerr << "error: failed to connect to session bus!" << endl;

        if (!connection.lastError().message().isEmpty())
            cerr << QString("error message: %1").arg(connection.lastError().message()).toUtf8().data() << endl;

        return 1;
    }

    if (!connection.registerService("org.BoxIt-32.Keyring")) {
        cerr << "error: failed to register DBus Service 'org.BoxIt-32.Keyring'!" << endl;

        if (!connection.lastError().message().isEmpty())
            cerr << QString("error message: %1").arg(connection.lastError().message()).toUtf8().data() << endl;

        return 1;
    }

    if (!connection.registerObject("/", &app)) {
        cerr << "error: failed to register DBus Object '/'!" << endl;

        if (!connection.lastError().message().isEmpty())
            cerr << QString("error message: %1").arg(connection.lastError().message()).toUtf8().data() << endl;

        return 1;
    }

    return app.exec();
}
