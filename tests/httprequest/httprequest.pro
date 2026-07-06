# Unit tests for HttpRequestConfig (cURL parse/emit).
# Built standalone: `cd tests/httprequest && qmake6 && make`
# Run with:        `QT_QPA_PLATFORM=offscreen ./tst_httprequest`

QT += testlib network widgets

CONFIG += console testcase
CONFIG -= app_bundle

TARGET = tst_httprequest

INCLUDEPATH += ../..

SOURCES += \
    test_httprequest.cpp \
    ../../httprequestconfig.cpp \
    ../../httprequestconfigwidget.cpp \
    ../../httprequestconfigdialog.cpp

HEADERS += \
    ../../httprequestconfig.h \
    ../../httprequestconfigwidget.h \
    ../../httprequestconfigdialog.h
