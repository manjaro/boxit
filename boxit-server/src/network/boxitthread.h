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

#ifndef BOXITTHREAD_H
#define BOXITTHREAD_H

#include <QString>
#include <QThread>
#include "const.h"
#include "boxitinstance.h"


class BoxitThread : public QThread
{
    Q_OBJECT
public:
    explicit BoxitThread(int socketDescriptor, QObject *parent = 0);
    ~BoxitThread();

    void run();

signals:
    void error(QString errorString);

private:
    int socketDescriptor;

};

#endif // BOXITTHREAD_H
