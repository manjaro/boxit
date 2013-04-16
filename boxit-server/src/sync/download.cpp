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

#include "download.h"

Download::Download(const QString destPath, QObject *parent) :
    QObject(parent),
    destPath(destPath)
{
    busy = false;
    error = false;
    attempts = 0;
    reply = NULL;
}



Download::~Download() {
    cancel();
}



bool Download::download(const QString url, bool keepAttempts) {
    if (busy)
        return false;

    if (file.isOpen()) {
        file.close();
        file.remove();
    }

    this->url = url;
    file.setFileName(destPath + "/" + QFileInfo(QUrl(url).path()).fileName());

    if (file.fileName().isEmpty()
            || (file.exists() && !file.remove())
            || !file.open(QIODevice::WriteOnly))
        return false;


    if (!keepAttempts)
        attempts = 0;

    errorStr.clear();
    error = false;
    reply = manager.get(QNetworkRequest(QUrl(url)));
    busy = true;

    connect(reply, SIGNAL(finished()),
            this, SLOT(finishedDownload()));
    connect(reply, SIGNAL(readyRead()),
            this, SLOT(readyRead()));
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)),
            this, SLOT(downloadProgress(qint64,qint64)));

    return true;
}



bool Download::isActive() {
    return busy;
}



void Download::cancel() {
    if (!busy || !reply)
        return;

    busy = false;
    error = true;
    errorStr = "download canceled!";

    reply->abort();
    reply->deleteLater();
    reply = NULL;

    if (file.isOpen())
        file.close();

    if (file.exists())
        file.remove();

    emit finished(false);
}



QString Download::lastError() {
    return errorStr;
}



bool Download::hasError() {
    return error;
}



//###
//### Private slots
//###



void Download::finishedDownload() {
    if (file.isOpen())
        file.close();

    if (busy) {
        busy = false;

        if (reply->error() != QNetworkReply::NoError) {
            error = true;
            errorStr = reply->errorString();
        }

        reply->deleteLater();
        reply = NULL;
    }
    else {
        error = true;
        errorStr = "Network reply is already destroyed!?";
    }

    if (error && file.exists())
        file.remove();

    // Retry if this was the first attempt
    if (error && attempts < RETRYATTEMPTS) {
        ++attempts;
        download(url, true);
        return;
    }

    emit finished(!error);
}



void Download::readyRead() {
    if (file.isOpen())
        file.write(reply->readAll());
    else
        cancel();
}



void Download::downloadProgress(qint64,qint64) {
}
