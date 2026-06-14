# Black-box tests for QJsonDiff comparison logic.
# Built standalone: `cd tests/compare && qmake6 && make`
# Run with:        `QT_QPA_PLATFORM=offscreen ./tst_compare`

QT += testlib widgets network

CONFIG += console testcase
CONFIG -= app_bundle

TARGET = tst_compare

INCLUDEPATH += ../..

SOURCES += \
    test_compare.cpp \
    ../../qjsondiff.cpp \
    ../../qjsoncontainer.cpp \
    ../../qjsonmodel.cpp \
    ../../qjsonitem.cpp \
    ../../jsondiffengine.cpp \
    ../../jsonitemdelegate.cpp \
    ../../jsonsyntaxhighlighter.cpp \
    ../../preferences/preferences.cpp

HEADERS += \
    ../../qjsondiff.h \
    ../../qjsoncontainer.h \
    ../../qjsonmodel.h \
    ../../qjsonitem.h \
    ../../jsondiffengine.h \
    ../../jsonitemdelegate.h \
    ../../jsonsyntaxhighlighter.h \
    ../../preferences/preferences.h

RESOURCES += ../../qjsontreeview.qrc

LIBS += -lz
