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

#include "timeoutreset.h"

TimeOutReset::TimeOutReset(BoxitSocket *socket, QObject *parent) :
    QThread(parent)
{
    this->socket = socket;
}



TimeOutReset::~TimeOutReset() {
    stop();
}



void TimeOutReset::stop() {
    if (!isRunning())
        return;

    terminate();
    wait();
}



void TimeOutReset::run() {
    while (true) {
        sleep(10);

        if (socket->state() != QAbstractSocket::ConnectedState)
            return;

        socket->sendData(MSG_RESET_TIMEOUT);
    }
}
