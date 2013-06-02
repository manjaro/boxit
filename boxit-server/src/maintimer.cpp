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

#include "maintimer.h"


MainTimer::MainTimer(QObject *parent) :
    QThread(parent)
{
}



MainTimer::~MainTimer() {
    if (isRunning()) {
        terminate();
        wait();
    }
}



void MainTimer::run() {
    int minutes = 0;

    while (true) {
        minutes += 10;

        // Set each 10 minutes a new check state
        setNewCheckState();

        // Run this each 3 hours
        if (minutes >= 180) {
            Database::removeOrphanPoolFiles();
            minutes = 0;
        }

        // Sleep 10 minutes
        sleep(600);
    }
}



void MainTimer::setNewCheckState() {
    QFile file(Global::getConfig().repoDir + "/" + BOXIT_STATE_FILE);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        cerr << "error: failed to save '" << file.fileName().toUtf8().data() << "'!" << endl;
        return;
    }

    QTextStream out(&file);
    out << "###\n### BoxIt global state file\n###\n";
    out << "\n# Unique hash code representing current repository state.\n# This hash code changes in a frequent interval.";
    out << "\nstate=" << QString(QCryptographicHash::hash(QString(QDateTime::currentDateTime().toString(Qt::ISODate) + QString::number(qrand())).toLocal8Bit(), QCryptographicHash::Sha1).toHex());
    out << "\n\n# Date and time of the last state update.";
    out << "\ndate=" << QDateTime::currentDateTime().toString(Qt::ISODate);

    file.close();

    // Fix file permission
    Global::fixFilePermission(file.fileName());
}
