#-------------------------------------------------
#
# Project created by QtCreator 2017-02-21T11:12:47
#
#-------------------------------------------------

QT       += core gui network charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = OAACARS
TEMPLATE = app
RC_ICONS = icons/OAAE_Star_Blue_trans.png.ico
ICON = icons/OAAE_Star_Blue_trans.png.icns

SOURCES += main.cpp\
        mainwindow.cpp \
    va.cpp \
    events.cpp \
    update.cpp \
    charts.cpp \
    airports.cpp

HEADERS  += mainwindow.h \
    va.h \
    charts.h \
    airports.h

FORMS    += mainwindow.ui

DISTFILES += \
    airports.sh \
    airports.csv \
    LICENSE.md

LIBS += -L$$PWD -lcurl

win32:LIBS += -lz -lws2_32
