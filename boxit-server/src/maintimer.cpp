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


void MainTimer::run() {
    while (true) {
        QString poolDir = Global::getConfig().poolDir;

        // Check for old packages in pool
        QStringList list = QDir(poolDir).entryList(QDir::Files | QDir::NoDotAndDotDot);
        QStringList filesToRemove;

        for (int i = 0; i < list.size(); ++i) {
            QString file = list.at(i);

            // Check if this is an orphan file
            if (RepoDB::fileExistsInRepos(file))
                continue;

            // Check when it was last modified
            QString filePath = poolDir + "/" + file;
            QFileInfo info(filePath);

            if (info.lastModified().daysTo(QDateTime::currentDateTime()) >= BOXIT_REMOVE_ORPHANS_AFTER_DAYS)
                filesToRemove.append(filePath);
        }

        if (!filesToRemove.isEmpty() && Pool::lock()) {
            for (int i = 0; i < filesToRemove.size(); ++i)
                QFile::remove(filesToRemove.at(i));

            Pool::unlock();
        }

        sleep(10800);
    }
}
