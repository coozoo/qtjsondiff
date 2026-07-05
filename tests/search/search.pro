# Search regression tests for QJsonContainer::findModelText.
# Locks in current search behavior BEFORE ghost/phantom rows land in
# the model so we can prove alignment doesn't break find.
#
# Built standalone: `cd tests/search && qmake6 && make`
# Run with:        `QT_QPA_PLATFORM=offscreen ./tst_search`

QT += testlib widgets network

CONFIG += console testcase
CONFIG -= app_bundle

TARGET = tst_search

INCLUDEPATH += ../..

SOURCES += \
    test_search.cpp \
    ../../qjsoncontainer.cpp \
    ../../qjsonmodel.cpp \
    ../../qjsonitem.cpp \
    ../../jsonitemdelegate.cpp \
    ../../jsonsyntaxhighlighter.cpp \
    ../../preferences/preferences.cpp

HEADERS += \
    ../../qjsoncontainer.h \
    ../../qjsonmodel.h \
    ../../qjsonitem.h \
    ../../jsonitemdelegate.h \
    ../../jsonsyntaxhighlighter.h \
    ../../preferences/preferences.h

RESOURCES += ../../qjsontreeview.qrc

LIBS += -lz
