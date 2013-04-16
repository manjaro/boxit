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

#include "boxitthread.h"


BoxitThread::BoxitThread(const int socketDescriptor, const int sessionID, QObject *parent) :
    QThread(parent),
    socketDescriptor(socketDescriptor),
    sessionID(sessionID)
{
}



BoxitThread::~BoxitThread() {
}



void BoxitThread::run() {
    BoxitInstance boxitSocket(sessionID);

    if (!boxitSocket.setSocketDescriptor(socketDescriptor))
        return;

    connect(&boxitSocket, SIGNAL(disconnected())    ,   this, SLOT(quit()));

    boxitSocket.setPrivateKey(Global::getConfig().sslKey);
    boxitSocket.setLocalCertificate(Global::getConfig().sslCertificate);

    boxitSocket.startServerEncryption();

    if (!boxitSocket.waitForEncrypted(30000))
        return;

    exec();
}

