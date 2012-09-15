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
#include <QDir>
#include <QCryptographicHash>
#include <iostream>
#include "global.h"
#include "network/boxitserver.h"
#include "repo/repodb.h"
#include "maintimer.h"

using namespace std;




bool addUser() {
    string username, password;
    cout << "username: ";
    getline (cin, username);
    cout << "password: ";
    getline (cin, password);

    User user;
    user.setUsername(QString::fromStdString(username));
    user.setPassword(QString(QCryptographicHash::hash(QString::fromStdString(password).toLocal8Bit(), QCryptographicHash::Sha1).toHex()));

    if (UserDatabase::getAllUsersData().contains(user)) {
        cerr << "error: username already exist!" << endl;
        return false;
    }
    else if (!UserDatabase::setUserData(user)) {
        cerr << "error: failed to add user!" << endl;
        return false;
    }
    else {
        cout << "added user!" << endl;
        return true;
    }
}



void printHelp() {
    cout << "\nboxit-server [OPTION]\n" << endl;
    cout << "\t-h/--help\t\tshow help" << endl;
    cout << "\t--adduser\t\tadd user to database\n" << endl;
}



int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    BoxitServer boxitServer(&app);
    MainTimer mainTimer;

    // Read config
    if (!Global::readConfig()) {
        cerr << "error: failed to read BoxIt config or config is incomplete!" << endl;
        return 1;
    }

    // Check arguments
    if (app.arguments().last() == "--adduser") {
        if (addUser())
            return 0;
        else
            return 1;
    }
    else if (app.arguments().last() == "-h" || app.arguments().last() == "--help") {
        printHelp();
        return 0;
    }



    // Fill repo
    cout << "reading database..." << endl;
    RepoDB::initRepoDB();


    // Start main timer
    cout << "starting main timer..." << endl;
    mainTimer.start();


    // Start server
    cout << "starting server..." << endl;
    if (!boxitServer.start()) {
        cerr << "error: failed to start server!" << endl;
        return 1;
    }

    cout << "running..." << endl;
    return app.exec();
}
