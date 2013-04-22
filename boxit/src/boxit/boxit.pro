#-------------------------------------------------
#
# Project created by QtCreator 2012-06-13T23:28:30
#
#-------------------------------------------------

QT       += core network dbus
QT       -= gui

LIBS    += -lreadline

TARGET = boxit
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    boxitsocket.cpp \
    version.cpp \
    timeoutreset.cpp \
    interactiveprocess.cpp

HEADERS += \
    boxitsocket.h \
    const.h \
    version.h \
    timeoutreset.h \
    interactiveprocess.h


target.path = /usr/bin

INSTALLS += target

RESOURCES += \
    resources.qrc




unix:!macx: LIBS += -L$$OUT_PWD/../DBus/ -lDBus

INCLUDEPATH += $$PWD/../DBus
DEPENDPATH += $$PWD/../DBus

unix:!macx: PRE_TARGETDEPS += $$OUT_PWD/../DBus/libDBus.a
