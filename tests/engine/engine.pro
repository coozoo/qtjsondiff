# Engine-direct unit tests for JsonDiffEngine.
# No QApplication, no widgets — just QtCore + QtTest + QtGui (for QColor
# pulled in transitively from preferences.h).

QT += testlib widgets

CONFIG += console testcase
CONFIG -= app_bundle

TARGET = tst_engine

INCLUDEPATH += ../..

SOURCES += \
    test_engine.cpp \
    ../../jsondiffengine.cpp \
    ../../qjsonmodel.cpp \
    ../../qjsonitem.cpp \
    ../../preferences/preferences.cpp

HEADERS += \
    ../../jsondiffengine.h \
    ../../qjsonmodel.h \
    ../../qjsonitem.h \
    ../../preferences/preferences.h
