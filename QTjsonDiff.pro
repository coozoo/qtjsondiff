#-------------------------------------------------
#
# Project created by QtCreator 2017-04-27T12:05:13
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QTjsonDiff
TRANSLATIONS = QTjsonDiff_en.ts QTjsonDiff_uk_UA.ts
TEMPLATE = app

CONFIG += c++11
CONFIG += openssl-linked
CONFIG += lrelease

SOURCES += main.cpp\
        mainwindow.cpp \
    qjsondiff.cpp \
    qjsonitem.cpp \
    qjsonmodel.cpp \
    qjsoncontainer.cpp

HEADERS  += mainwindow.h \
    qjsoncontainer.h \
    qjsondiff.h \
    qjsonitem.h \
    qjsonmodel.h

FORMS    += mainwindow.ui

LIBS += -lz

win32:RC_FILE = myapp.rc
macx:RC_FILE = computer.icns

DISTFILES +=

RESOURCES += \
    qjsontreeview.qrc

binary.files += $$TARGET
binary.path = /usr/bin
translations.files += $$files(.qm/*.qm,true)
translations.path = /usr/share/$$TARGET
icon.files +=diff.png
icon.path += /usr/share/icons
desktop.files += qtjsondiff.desktop
desktop.path += /usr/share/applications/
INSTALLS += binary translations icon desktop
