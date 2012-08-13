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

#ifndef TIMEOUTRESET_H
#define TIMEOUTRESET_H

#include <QObject>
#include <QThread>
#include <iostream>
#include "boxitsocket.h"
#include "const.h"

using namespace std;


class TimeOutReset : public QThread
{
    Q_OBJECT
public:
    explicit TimeOutReset(BoxitSocket *socket, QObject *parent = 0);
    ~TimeOutReset();
    void stop();
    void run();

private:
    BoxitSocket *socket;

};

#endif // TIMEOUTRESET_H
