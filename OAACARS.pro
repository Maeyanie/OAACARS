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
    va.cpp \
    events.cpp \
    update.cpp \
    airports.cpp

HEADERS  += mainwindow.h \
    va.h \
    airports.h

FORMS    += mainwindow.ui

DISTFILES += \
    notes.txt \
    airports.sh \
    airports.csv

LIBS += -L$$PWD -lcurl

win32:LIBS += -lz -lws2_32
