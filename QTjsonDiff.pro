#-------------------------------------------------
#
# Project created by QtCreator 2017-04-27T12:05:13
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QTjsonDiff
#TRANSLATIONS = qtjsondiff_en.ts qtjsondiff_uk_UA.ts
#CODECFORSRC = UTF-8
TEMPLATE = app

CONFIG += c++11
CONFIG += openssl-linked
win32{
    !contains(QMAKE_TARGET.arch, x86_64){
        LIBS += -LC:/OpenSSL-Win64/lib -lubsec
        INCLUDEPATH += C:/OpenSSL-Win64/include
    } else {
        LIBS += -LC:/OpenSSL-Win32/lib -lubsec
        INCLUDEPATH += C:/OpenSSL-Win32/include
    }
}

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
