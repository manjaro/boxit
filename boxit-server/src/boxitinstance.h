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

#ifndef BOXITINSTANCE_H
#define BOXITINSTANCE_H

#include <QString>
#include <QList>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QByteArray>
#include <QTimer>
#include <QCryptographicHash>
#include "network/boxitsocket.h"
#include "const.h"
#include "user/user.h"
#include "user/userdbs.h"
#include "repo/repo.h"
#include "repo/repodb.h"
#include "repo/pool.h"


class BoxitInstance : public BoxitSocket
{
    Q_OBJECT
public:
    BoxitInstance();
    ~BoxitInstance();

private:
    int loginCount, timeoutCount;
    bool sendMessages, poolLocked;
    User user;
    QTimer timeOutTimer;
    QFile file;
    QByteArray fileCheckSum;

    bool basicRepoChecks(QByteArray &data, QStringList &split, int minimumLenght = 2);
    QByteArray sha1CheckSum(QString filePath);

private slots:
    void sendFinished(bool success);
    void sendMessage(QString msg);
    void read_Data(quint16 msgID, QByteArray data);
    void timeOutDestroy();

};


#endif // BOXITINSTANCE_H
