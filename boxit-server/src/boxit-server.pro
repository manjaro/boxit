#-------------------------------------------------
#
# Project created by QtCreator 2012-06-13T23:49:22
#
#-------------------------------------------------

QT       += core network

QT       -= gui

TARGET = boxit-32-server
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
    global.cpp \
    sync/sync.cpp \
    sync/download.cpp \
    sync/sha256/sha256.c \
    sync/sha256/cryptsha256.cpp \
    maintimer.cpp \
    db/database.cpp \
    db/branch.cpp \
    db/repo.cpp \
    db/status.cpp

HEADERS += \
    network/boxitthread.h \
    network/boxitsocket.h \
    network/boxitserver.h \
    const.h \
    boxitinstance.h \
    user/userdbs.h \
    user/user.h \
    global.h \
    sync/sync.h \
    sync/download.h \
    sync/sha256/sha256.h \
    sync/sha256/cryptsha256.h \
    maintimer.h \
    db/database.h \
    db/branch.h \
    db/repo.h \
    db/status.h


target.path = /usr/bin

daemonscript.path = /etc/init.d
daemonscript.files = ../scripts/etc/init.d/*

configs.path = /etc/boxit
configs.files = ../scripts/etc/boxit/*

binscripts.path = /usr/bin
binscripts.files = ../scripts/usr/bin/*

INSTALLS += target daemonscript binscripts configs
