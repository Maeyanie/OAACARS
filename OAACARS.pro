#-------------------------------------------------
#
# Project created by QtCreator 2017-02-21T11:12:47
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = OAACARS
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    va.cpp

HEADERS  += mainwindow.h \
    va.h

FORMS    += mainwindow.ui

DISTFILES += \
    notes.txt

LIBS += -L$$PWD -lcurl

win32:LIBS += -lz -lws2_32
