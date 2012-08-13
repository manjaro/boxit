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

#ifndef BOXITSOCKET_H
#define BOXITSOCKET_H

#include <QString>
#include <QSslSocket>
#include <QByteArray>
#include <QDataStream>
#include <QList>
#include <iostream>
#include "const.h"

using namespace std;


class BoxitSocket : public QSslSocket
{
    Q_OBJECT
public:
    explicit BoxitSocket(QObject *parent = 0);

    void sendData(quint16 msgID);
    void sendData(quint16 msgID, QByteArray data);

signals:
    void readData(quint16 msgID, QByteArray data);

private slots:
    void readyReadData();
    void socketError();
    void sslErrors(const QList<QSslError> &errors);

private:
    quint16 readBlockSize, readMSGID;

};

#endif // BOXITSOCKET_H
