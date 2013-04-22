#-------------------------------------------------
#
# Project created by QtCreator 2013-04-22T17:24:36
#
#-------------------------------------------------

QT       += core dbus
QT       -= gui

TARGET = boxit-keyring
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp

target.path = /usr/bin

INSTALLS += target




unix:!macx: LIBS += -L$$OUT_PWD/../DBus/ -lDBus

INCLUDEPATH += $$PWD/../DBus
DEPENDPATH += $$PWD/../DBus

unix:!macx: PRE_TARGETDEPS += $$OUT_PWD/../DBus/libDBus.a

