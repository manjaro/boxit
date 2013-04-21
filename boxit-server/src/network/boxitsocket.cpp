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

#include "boxitsocket.h"

BoxitSocket::BoxitSocket(int sessionID, QObject *parent) :
    QSslSocket(parent),
    sessionID(sessionID)
{
    setReadBufferSize(0);
    setPeerVerifyMode(QSslSocket::VerifyNone);

    readBlockSize = 0;
    readMSGID = 0;
    timeoutCount = 0;
    timeOutTimer.setInterval(5000);

    // Connect signals and slots
    connect(this, SIGNAL(readyRead())   ,   this, SLOT(readyReadData()));
    connect(this, SIGNAL(sslErrors(QList<QSslError>))   ,   this, SLOT(sslErrors(QList<QSslError>)));
    connect(this, SIGNAL(error(QAbstractSocket::SocketError))   ,   this, SLOT(socketError()));
    connect(&timeOutTimer, SIGNAL(timeout())    ,   this, SLOT(timeOutDestroy()));

    // Start timeout timer
    timeOutTimer.start();
}



void BoxitSocket::readyReadData() {
    QDataStream in(this);
    in.setVersion(QDataStream::Qt_4_6);


    if (readBlockSize == 0) {
        if (bytesAvailable() < (int)sizeof(quint16))
            return;

        in >> readBlockSize;
    }


    if (readMSGID == 0) {
        if (bytesAvailable() < (int)sizeof(quint16))
            return;

        in >> readMSGID;
    }


    if (bytesAvailable() < readBlockSize)
        return;


    QByteArray subData;
    in >> subData;
    data.append(subData);

    // Restart our timeout
    timeoutCount = 0;

    // Emit signal
    if (readMSGID != MSG_RESET_TIMEOUT
            && readMSGID != MSG_DATA_PACKAGE_MULTIPLE)
        emit readData(readMSGID, data);

    // Clear data if this isn't a multiple package
    if (readMSGID != MSG_DATA_PACKAGE_MULTIPLE)
        data.clear();

    readBlockSize = 0;
    readMSGID = 0;

    if (bytesAvailable() != 0)
        readyReadData();
}



void BoxitSocket::sendData(quint16 msgID) {
    sendData(msgID, QByteArray());
}



void BoxitSocket::sendData(quint16 msgID, QByteArray data) {
    // Send data in multiple data packages, if data is too big...
    while (true) {
        QByteArray subData = data.mid(0, BOXIT_SOCKET_MAX_SIZE);
        data.remove(0, BOXIT_SOCKET_MAX_SIZE);

        quint16 subMsgID = msgID;
        if (!data.isEmpty())
            subMsgID = MSG_DATA_PACKAGE_MULTIPLE;

        // Send data
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_4_6);
        out << (quint16)0;
        out << (quint16)subMsgID;
        out << subData;
        out.device()->seek(0);
        out << (quint16)(block.size() - 2*sizeof(quint16));

        write(block);
        flush();

        waitForBytesWritten(2000);

        if (data.isEmpty())
            break;
    }
}



//###
//### Slots
//###



void BoxitSocket::socketError() {
    if (error() == QAbstractSocket::RemoteHostClosedError)
        return;

    cerr << errorString().toUtf8().data() << endl;
}



void BoxitSocket::sslErrors(const QList<QSslError> &errors) {
    bool first = true;

    foreach (QSslError error, errors)
    {
        if (error.error() == QSslError::HostNameMismatch || error.error() == QSslError::SelfSignedCertificate)
            continue;

        if (first) {
             cerr << "SSL error(s): ";
             first = false;
        }

        cerr << error.errorString().toUtf8().data() << endl;
    }
}



void BoxitSocket::timeOutDestroy() {
    timeoutCount += 5;

    // Disconnect after 3 minutes...
    if (timeoutCount >= 180)
        disconnectFromHost();
}
