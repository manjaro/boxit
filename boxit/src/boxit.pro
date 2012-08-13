#-------------------------------------------------
#
# Project created by QtCreator 2012-06-13T23:28:30
#
#-------------------------------------------------

QT       += core network
QT       -= gui

LIBS    += -lreadline

TARGET = boxit
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    boxitsocket.cpp \
    version.cpp \
    interactiveprocess.cpp \
    timeoutreset.cpp

HEADERS += \
    boxitsocket.h \
    const.h \
    version.h \
    interactiveprocess.h \
    timeoutreset.h


target.path = /usr/bin

INSTALLS += target
