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

#include "interactiveprocess.h"


int InteractiveProcess::stdinClone = 0; // readline crashes on autocompletion with a value of -1!? Solved it with a boolean...
bool InteractiveProcess::initialized = false;



InteractiveProcess::InteractiveProcess(QObject *parent)
    : QProcess(parent)
{
    setProcessChannelMode(QProcess::ForwardedChannels);

    if (!initialized) {
        initialized = true;
        stdinClone = ::dup(fileno(stdin));
    }
}



void InteractiveProcess::setupChildProcess()
{
    ::dup2(stdinClone, fileno(stdin));
}




