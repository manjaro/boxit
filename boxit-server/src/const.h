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


#define BOXIT_VERSION 5
#define BOXIT_PORT 59872
#define BOXIT_SPLIT_CHAR "|"
#define BOXIT_SOCKET_MAX_SIZE 50000
#define BOXIT_SERVER_CONFIG "/etc/boxit/boxit.conf"
#define BOXIT_USER_DBFILE "/etc/boxit/users.db"
#define BOXIT_TMP "/var/repo/boxit_tmp"
#define BOXIT_SESSION_TMP "/var/repo/boxit_tmp/sessions"
#define BOXIT_STATUS_TMP "/var/repo/boxit_tmp/status"
#define BOXIT_DB_CONFIG ".config"
#define BOXIT_DB_SYNC_EXCLUDE ".sync_exclude"
#define BOXIT_ARCHITECTURES "i686 x86_64"
#define BOXIT_OVERLAY_POOL "pool/overlay"
#define BOXIT_SYNC_POOL "pool/sync"
#define BOXIT_PACKAGE_FILTERS "*.pkg.tar.xz *.pkg.tar.gz"
#define BOXIT_SIGNATURE_ENDING ".sig"
#define BOXIT_DB_ENDING ".db.tar.gz"
#define BOXIT_DB_LINK_ENDING ".db"
#define BOXIT_DB_DESC_FILE "desc"
#define BOXIT_REMOVE_ORPHANS_AFTER_DAYS 3
#define BOXIT_BRANCH_STATE_FILE "state"
#define BOXIT_SYSTEM_USERNAME "system"
#define BOXIT_SYSTEM_SESSION_ID 1


// Socket IDs
#define MSG_INVALID 1
#define MSG_RESET_TIMEOUT 2
#define MSG_DATA_PACKAGE_MULTIPLE 3

#define MSG_SUCCESS 100
#define MSG_CHECK_VERSION 101
#define MSG_AUTHENTICATE 102
#define MSG_IS_LOCKED 103

#define MSG_GET_BRANCHES 110
#define MSG_DATA_BRANCH 111
#define MSG_GET_BRANCH_URL 112
#define MSG_SET_BRANCH_URL 113
#define MSG_GET_BRANCH_SYNC_EXCLUDE_FILES 114
#define MSG_SET_BRANCH_SYNC_EXCLUDE_FILES 115

#define MSG_GET_REPOS 120
#define MSG_GET_REPOS_WITH_PACKAGES 121
#define MSG_DATA_REPO 122
#define MSG_DATA_OVERLAY_PACKAGES 123
#define MSG_DATA_SYNC_PACKAGES 124

#define MSG_POOL_CHECK_FILES_EXISTS 130
#define MSG_LOCK_POOL_FILES 131
#define MSG_MOVE_POOL_FILES 132
#define MSG_RELEASE_POOL_LOCK 133

#define MSG_LOCK_REPO 140
#define MSG_APPLY_REPO_CHANGES 141
#define MSG_RELEASE_REPO_LOCK 142

#define MSG_LISTEN_ON_STATUS 150
#define MSG_DATA_NEW_STATUS_LIST 151
#define MSG_DATA_END_STATUS_LIST 152
#define MSG_DATA_BRANCH_STATUS 153
#define MSG_DATA_REPO_STATUS 154
#define MSG_STOP_LISTEN_ON_STATUS 155
#define MSG_STATUS_SESSION_FINISHED 156
#define MSG_STATUS_SESSION_FAILED 157

#define MSG_SYNC_BRANCH 160
#define MSG_SET_PASSWD 161
#define MSG_SNAP_BRANCH 162

#define MSG_FILE_CHECKSUM 170
#define MSG_FILE_UPLOAD 171
#define MSG_FILE_ALREADY_EXISTS 172
#define MSG_FILE_ALREADY_EXISTS_WRONG_CHECKSUM 173
#define MSG_DATA_UPLOAD 174
#define MSG_DATA_UPLOAD_FINISH 175

#define MSG_ERROR 200
#define MSG_ERROR_NOT_AUTHORIZED 201
#define MSG_ERROR_WRONG_CHECKSUM 202
#define MSG_ERROR_WRONG_PASSWORD 203

#endif // CONST_H
