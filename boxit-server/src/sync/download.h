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

#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QUrl>
#include <QFileInfo>


#define RETRYATTEMPTS 1


class Download : public QObject
{
    Q_OBJECT
public:
    explicit Download(const QString destPath, QObject *parent = 0);
    ~Download();

    bool download(const QString url, bool keepAttempts = false);
    bool isActive();
    void cancel();
    QString lastError();
    bool hasError();

signals:
    void finished(bool success);

private:
    const QString destPath;
    bool busy, error;
    int attempts;
    QNetworkAccessManager manager;
    QNetworkReply *reply;
    QFile file;
    QString errorStr, url;

private slots:
    void finishedDownload();
    void readyRead();
    void downloadProgress(qint64,qint64);

};

#endif // DOWNLOAD_H
