#-------------------------------------------------
#
# Project created by QtCreator 2012-06-13T23:49:22
#
#-------------------------------------------------

QT       += core network

QT       -= gui

TARGET = boxit-server
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    network/boxitthread.cpp \
    network/boxitsocket.cpp \
    network/boxitserver.cpp \
    boxitinstance.cpp \
    user/userdbs.cpp \
    user/user.cpp \
    repo/repodb.cpp \
    repo/repo.cpp \
    global.cpp \
    repo/repobase.cpp \
    repo/repothread.cpp \
    sync/sync.cpp \
    sync/download.cpp \
    sync/sha256/sha256.c \
    sync/sha256/cryptsha256.cpp

HEADERS += \
    network/boxitthread.h \
    network/boxitsocket.h \
    network/boxitserver.h \
    const.h \
    boxitinstance.h \
    user/userdbs.h \
    user/user.h \
    repo/repodb.h \
    repo/repo.h \
    global.h \
    repo/repobase.h \
    repo/repothread.h \
    sync/sync.h \
    sync/download.h \
    sync/sha256/sha256.h \
    sync/sha256/cryptsha256.h


target.path = /usr/bin

daemonscript.path = /etc/init.d
daemonscript.files = ../scripts/etc/init.d/*

configs.path = /etc/boxit
configs.files = ../scripts/etc/boxit/*

binscripts.path = /usr/bin
binscripts.files = ../scripts/usr/bin/*

INSTALLS += target daemonscript binscripts configs
