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

#include "boxitserver.h"

BoxitServer::BoxitServer(QObject *parent) :
    QTcpServer(parent)
{
}



bool BoxitServer::start() {
    if (!QSslSocket::supportsSsl()) {
        cerr << "*** Your version of Qt does not support SSL ***\nYou must obtain a version of Qt that has SSL support enabled. If you believe that your version of Qt has SSL support enabeld, you may need to install the OpenSSL run-time libraries." << endl;
        return false;
    }

    return listen(QHostAddress::Any, (quint16)BOXIT_PORT);
}



void BoxitServer::incomingConnection(int socketDescriptor)
{
    BoxitThread *thread = new BoxitThread(socketDescriptor, this);
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(thread, SIGNAL(terminated()), thread, SLOT(deleteLater()));
    thread->start();
}

