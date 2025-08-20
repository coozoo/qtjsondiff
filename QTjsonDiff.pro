#-------------------------------------------------
#
# Project created by QtCreator 2017-04-27T12:05:13
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QTjsonDiff
TRANSLATIONS = ./translations/$${TARGET}_en_US.ts ./translations/$${TARGET}_uk_UA.ts
TEMPLATE = app

CONFIG += c++17
CONFIG += openssl-linked
CONFIG += lrelease
# do not show qDebug() messages in release builds
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT

SOURCES += main.cpp\
    commandlineparser.cpp \
    jsonsyntaxhighlighter.cpp \
        mainwindow.cpp \
    preferences/preferences.cpp \
    preferences/preferencesdialog.cpp \
    qjsondiff.cpp \
    qjsonitem.cpp \
    qjsonmodel.cpp \
    qjsoncontainer.cpp

HEADERS  += mainwindow.h \
    commandlineparser.h \
    jsonsyntaxhighlighter.h \
    preferences/preferences.h \
    preferences/preferencesdialog.h \
    qjsoncontainer.h \
    qjsondiff.h \
    qjsonitem.h \
    qjsonmodel.h

FORMS    += mainwindow.ui \
    preferences/preferencesdialog.ui

LIBS += -lz

win32:RC_FILE = myapp.rc
macx:RC_FILE = computer.icns

DISTFILES +=

RESOURCES += \
    qjsontreeview.qrc

binary.files += $$TARGET
binary.path = /usr/bin
translations.files += ./translations/$$files(*.qm/*.qm,true)
translations.path = /usr/share/$$TARGET
icon.files +=diff.png
icon.path += /usr/share/icons/hicolor/scalable/apps
desktop.files += qtjsondiff.desktop
desktop.path += /usr/share/applications/
INSTALLS += binary translations icon desktop
