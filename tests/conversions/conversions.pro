# Unit tests for QJsonModel + QJsonTreeItem.
# Built standalone: `cd tests/conversions && qmake6 && make`
# Run with:        `QT_QPA_PLATFORM=offscreen ./tst_json_conversions`

QT += testlib widgets

CONFIG += console testcase
CONFIG -= app_bundle

TARGET = tst_json_conversions

# Pull headers in from the project root.
INCLUDEPATH += ../..

SOURCES += \
    test_json_conversions.cpp \
    ../../qjsonmodel.cpp \
    ../../qjsonitem.cpp \
    ../../preferences/preferences.cpp

HEADERS += \
    ../../qjsonmodel.h \
    ../../qjsonitem.h \
    ../../preferences/preferences.h
