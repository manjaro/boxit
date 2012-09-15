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

#ifndef CONST_H
#define CONST_H


#define BOXIT_VERSION 2
#define BOXIT_PORT 59872
#define BOXIT_SPLIT_CHAR "|"
#define BOXIT_SERVER_CONFIG "/etc/boxit/boxit.conf"
#define BOXIT_USER_DBFILE "/etc/boxit/users.db"
#define BOXIT_TMP "/tmp/boxit"
#define BOXIT_REPO_CONFIG ".config"
#define BOXIT_ARCHITECTURES "i686 x86_64"
#define BOXIT_POOL_REPO "pool"
#define BOXIT_FILE_FILTERS "*.pkg.tar.xz *.pkg.tar.gz"
#define BOXIT_SIGNATURE_ENDING ".sig"
#define BOXIT_SCRIPT "boxit-mount"
#define BOXIT_DB_ENDING ".db.tar.gz"
#define BOXIT_DB_LINK_ENDING ".db"
#define BOXIT_DB_DESC_FILE "desc"
#define BOXIT_FLUSH_STRING "#!f"
#define BOXIT_REMOVE_ORPHANS_AFTER_DAYS 5


// Socket IDs
#define MSG_INVALID 1
#define MSG_RESET_TIMEOUT 97
#define MSG_YES 98
#define MSG_NO 99
#define MSG_SUCCESS 100
#define MSG_CHECK_VERSION 101
#define MSG_AUTHENTICATE 102
#define MSG_REPOSITORY_STATE 103
#define MSG_PACKAGE_LIST 104
#define MSG_PACKAGE 105
#define MSG_SIGNATURE_LIST 106
#define MSG_SIGNATURE 107
#define MSG_LOCK 108
#define MSG_UNLOCK 109
#define MSG_REMOVE_PACKAGE 110
#define MSG_FILE_CHECKSUM 111
#define MSG_FILE_UPLOAD 112
#define MSG_DATA_UPLOAD 113
#define MSG_DATA_UPLOAD_FINISH 114
#define MSG_COMMIT 115
#define MSG_MESSAGE 116
#define MSG_GET_POOL_MESSAGES 117
#define MSG_STOP_POOL_MESSAGES 118
#define MSG_PROCESS_BACKGROUND 119
#define MSG_PROCESS_FINISHED 120
#define MSG_PROCESS_FAILED 121
#define MSG_PROCESS_RUNNING 122
#define MSG_REQUEST_PROCESS_STATE 123
#define MSG_FILE_ALREADY_EXISTS 124

#define MSG_ERROR 200
#define MSG_ERROR_NOT_AUTHORIZED 201
#define MSG_ERROR_NOT_EXIST 202
#define MSG_ERROR_POOL_ALREADY_LOCKED 203
#define MSG_ERROR_POOL_NOT_LOCKED 204
#define MSG_ERROR_CHECKSUM_WRONG 205


// Shell IDs
#define MSG_SET_PASSWD 300
#define MSG_REQUEST_LIST 301
#define MSG_LIST_REPO 302
#define MSG_REPO_ADD 303
#define MSG_REPO_GET_SYNC_URL 307
#define MSG_REPO_CHANGE_SYNC_URL 308
#define MSG_REPO_MERGE 309
#define MSG_REPO_SYNC 310
#define MSG_GET_EXCLUDE_CONTENT 311
#define MSG_SET_EXCLUDE_CONTENT 312
#define MSG_KILL 313
#define MSG_POOL_STATE 314

#define MSG_ERROR_WRONG_PASSWORD 400
#define MSG_ERROR_ALREADY_EXIST 401
#define MSG_ERROR_IS_BUSY 402
#define MSG_ERROR_NO_SYNC_REPO 403
#define MSG_ERROR_NOT_BUSY 404


#endif // CONST_H
