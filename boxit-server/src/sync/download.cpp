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

Download::Download(QObject *parent) :
    QObject(parent)
{
    isRunning = false;
    reply = NULL;

    // Connect signals and slots
    connect(&manager, SIGNAL(finished(QNetworkReply*))    ,   SLOT(fileDownloaded(QNetworkReply*)));
}



Download::~Download() {
    cancel();
}



bool Download::isActive() {
    return isRunning;
}



QString Download::lastError() {
    return errorStr;
}



bool Download::hasError() {
    return error;
}



bool Download::download(const QString url, const QString destPath) {
    if (isRunning)
        return false;

    attempts = 0;
    this->destPath = destPath;

    return _download(url);
}



void Download::cancel() {
    if (!isRunning || !reply)
        return;

    reply->abort();
    reply->deleteLater();
    reply = NULL;

    if (file.isOpen())
        file.close();

    if (file.exists())
        file.remove();

    isRunning = false;
    error = true;
    errorStr = "error: download canceled!";

    emit finished(false);
}




//###
//### Private
//###


bool Download::_download(const QString url) {
    // Clean up first
    errorStr.clear();
    error = false;

    // Set url and file path
    this->url = url;
    file.setFileName(destPath + "/" + QFileInfo(QUrl(url).path()).fileName());

    // Open file
    if ((file.exists() && !file.remove())
            || !file.open(QIODevice::WriteOnly)) {
        errorStr = "error: failed to open file!";
        return false;
    }

    // Create network reply
    reply = manager.get(QNetworkRequest(QUrl(url)));
    isRunning = true;

    // Connect signals and slots
    connect(reply, SIGNAL(readyRead()),
            this, SLOT(readyRead()));
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)),
            this, SLOT(downloadProgress(qint64,qint64)));

    return true;
}



void Download::readyRead() {
    file.write(reply->readAll());
}



void Download::downloadProgress(qint64,qint64) {
}



void Download::fileDownloaded(QNetworkReply *reply) {
    // Close file
    file.close();

    // Check for errors
    if (reply->error() != QNetworkReply::NoError) {
        error = true;
        errorStr = reply->errorString();
    }

    reply->deleteLater();
    this->reply = NULL;

    // remove file on error
    if (error && file.exists())
        file.remove();

    file.setFileName("");

    // Retry if this was the first attempt
    if (error && attempts < RETRYATTEMPTS) {
        ++attempts;
        _download(url);
        return;
    }

    // Reset value
    isRunning = false;

    // Emit signal
    emit finished(!error);
}
