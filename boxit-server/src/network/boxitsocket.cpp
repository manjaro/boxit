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

#include "boxitsocket.h"

BoxitSocket::BoxitSocket(QObject *parent) :
    QSslSocket(parent)
{
    setReadBufferSize(0);
    setPeerVerifyMode(QSslSocket::VerifyNone);

    readBlockSize = 0;
    readMSGID = 0;

    connect(this, SIGNAL(readyRead())   ,   this, SLOT(readyReadData()));
    connect(this, SIGNAL(sslErrors(QList<QSslError>))   ,   this, SLOT(sslErrors(QList<QSslError>)));
    connect(this, SIGNAL(error(QAbstractSocket::SocketError))   ,   this, SLOT(socketError()));
}



void BoxitSocket::readyReadData() {
    QDataStream in(this);
    in.setVersion(QDataStream::Qt_4_0);


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


    QByteArray data;
    in >> data;
    emit readData(readMSGID, data);

    readBlockSize = 0;
    readMSGID = 0;

    if (bytesAvailable() != 0)
        readyReadData();
}



void BoxitSocket::sendData(quint16 msgID) {
    sendData(msgID, QByteArray());
}



void BoxitSocket::sendData(quint16 msgID, QByteArray data) {
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_0);
    out << (quint16)0;
    out << (quint16)msgID;
    out << data;
    out.device()->seek(0);
    out << (quint16)(block.size() - 2*sizeof(quint16));

    write(block);
    flush();

    waitForBytesWritten(2000);
}



//###
//### Slots
//###



void BoxitSocket::socketError() {
  cerr << errorString().toAscii().data() << endl;
}



void BoxitSocket::sslErrors(const QList<QSslError> &errors) {
    QString errorStrings;
    foreach (QSslError error, errors)
    {
      errorStrings += error.errorString();
      if (error != errors.last())
      {
        errorStrings += '\n';
      }
    }

    cerr << errorStrings.toAscii().data() << endl;
}
