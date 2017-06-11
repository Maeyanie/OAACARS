#-------------------------------------------------
#
# Project created by QtCreator 2017-02-21T11:12:47
#
#-------------------------------------------------

QT       += core gui network
#QT       += multimediawidgets

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
    airports.cpp \
    ipdialog.cpp

HEADERS  += mainwindow.h \
    va.h \
    airports.h \
    ipdialog.h

FORMS    += mainwindow.ui \
    ipdialog.ui

DISTFILES += \
    airports.sh \
    airports.csv \
    LICENSE.md \
    android/AndroidManifest.xml \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradlew \
    android/res/values/libs.xml \
    android/build.gradle \
    android/gradle/wrapper/gradle-wrapper.properties \
    android/gradlew.bat

PLATFORM = $$QMAKESPEC
PLATFORM ~= s|.+/|
PLATFORM ~= s|.+\\|

LIBS += -L$$PWD/$$PLATFORM -L$$PWD -lcurl
win32:LIBS += -lz -lws2_32

ANDROID_EXTRA_LIBS = $$PWD/android-g++/libcurl.so
ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
