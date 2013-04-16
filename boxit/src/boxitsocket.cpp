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

BoxitSocket::BoxitSocket(QObject *parent) :
    QSslSocket(parent)
{
    setReadBufferSize(0);
    setPeerVerifyMode(QSslSocket::VerifyNone);

    connect(this, SIGNAL(disconnected())    ,   this, SLOT(hostDisconnected()));
    connect(this, SIGNAL(sslErrors(QList<QSslError>))   ,   this, SLOT(sslErrors(QList<QSslError>)));
    connect(this, SIGNAL(error(QAbstractSocket::SocketError))   ,   this, SLOT(socketError()));
}



bool BoxitSocket::connectToHost(const QString &hostName) {
    if (!QSslSocket::supportsSsl()) {
        cerr << "*** Your version of Qt does not support SSL ***\nYou must obtain a version of Qt that has SSL support enabled. If you believe that your version of Qt has SSL support enabeld, you may need to install the OpenSSL run-time libraries." << endl;
        return false;
    }

    connectToHostEncrypted(hostName, (quint16)BOXIT_PORT);
    if (!waitForConnected(30000))
        return false;

    return waitForEncrypted(30000);
}



void BoxitSocket::disconnectFromHost() {
    disconnect(this, SLOT(hostDisconnected()));
    QTcpSocket::disconnectFromHost();
}



bool BoxitSocket::readData(quint16 &msgID) {
    QByteArray data;
    return readData(msgID, data);
}



bool BoxitSocket::readData(quint16 &msgID, QByteArray &data) {
    data.clear();

    while (true) {
        QByteArray subData;
        quint16 blockSize;
        QDataStream in(this);
        in.setVersion(QDataStream::Qt_4_8);

        blockSize = msgID = 0;


        // Get block size
        while (bytesAvailable() < (int)sizeof(quint16)) {
            if (waitForReadyRead(-1))
                continue;

            if (bytesAvailable() < (int)sizeof(quint16))
                return false;
        }

        in >> blockSize;


        // Get msg ID
        while (bytesAvailable() < (int)sizeof(quint16)) {
            if (waitForReadyRead(-1))
                continue;

            if (bytesAvailable() < (int)sizeof(quint16))
                return false;
        }

        in >> msgID;


        // Get data
        while (bytesAvailable() < blockSize) {
            if (waitForReadyRead(-1))
                continue;

            if (bytesAvailable() < blockSize)
                return false;
        }

        in >> subData;
        data.append(subData);


        // Check if server received an invalid request
        if (msgID == MSG_INVALID) {
            cerr << "error: server has received an invalid request and has disconnected!" << endl;
            disconnectFromHost();
            exit(1);
        }

        // If this isn't a multiple package then break the loop
        if (msgID != MSG_DATA_PACKAGE_MULTIPLE)
            break;
    }

    return true;
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
        out.setVersion(QDataStream::Qt_4_0);
        out << (quint16)0;
        out << (quint16)subMsgID;
        out << subData;
        out.device()->seek(0);
        out << (quint16)(block.size() - 2*sizeof(quint16));

        write(block);
        flush();

        waitForBytesWritten(-1);

        if (data.isEmpty())
            break;
    }
}



//###
//### Slots
//###



void BoxitSocket::hostDisconnected() {
    cerr << "Connection lost - Maybe you have reached the timeout of 3 minutes?!" << endl;
    exit(1);
}


void BoxitSocket::socketError() {
  cerr << "error: " << errorString().toAscii().data() << endl;
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

        cerr << error.errorString().toAscii().data() << endl;
    }
}
