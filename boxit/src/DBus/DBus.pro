#-------------------------------------------------
#
# Project created by QtCreator 2013-04-22T18:56:07
#
#-------------------------------------------------

QT       += dbus
QT       -= gui

DEFINES += "UNIQUE_BUILD_HASH=\"\\\"$$system(openssl rand -base64 128)\\\"\""

TARGET = DBus
TEMPLATE = lib
CONFIG += staticlib

SOURCES += dbusclient.cpp \
    dbusserver.cpp \
    simplecrypt.cpp

HEADERS += dbusclient.h \
    dbusserver.h \
    simplecrypt.h
