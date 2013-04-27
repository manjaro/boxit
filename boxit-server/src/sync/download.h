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
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QFileInfo>
#include <QUrl>


#define RETRYATTEMPTS 1


class Download : public QObject
{
    Q_OBJECT
public:
    explicit Download(QObject *parent = 0);
    ~Download();

    bool download(const QString url, const QString destPath);
    void cancel();
    bool isActive();
    QString lastError();
    bool hasError();

signals:
    void finished(bool success);

private:
    QNetworkAccessManager manager;
    QNetworkReply *reply;
    QString destPath, errorStr, url;
    QFile file;
    int attempts;
    bool isRunning, error;

    bool _download(const QString url);

private slots:
    void fileDownloaded(QNetworkReply *reply);
    void readyRead();
    void downloadProgress(qint64,qint64);

};

#endif // DOWNLOAD_H
