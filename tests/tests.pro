# This project file builds the unit test executable.

# Add the necessary Qt modules.
QT += testlib widgets

# Configure this as a console application for testing.
CONFIG += console testcase
CONFIG -= app_bundle

# Name the test executable. This does not conflict with your main TARGET.
TARGET = tst_json_conversions

# Add the path to your main application's source code so the compiler
# can find the headers (e.g., #include "qjsonmodel.h").
INCLUDEPATH += ../

# Define the source files needed for this test executable.
SOURCES += \
    test_json_conversions.cpp \
    ../qjsonmodel.cpp \
    ../qjsonitem.cpp \
    ../preferences/preferences.cpp # <-- THIS IS THE FIX

# Define the headers needed for this test executable.
HEADERS += \
    ../qjsonmodel.h \
    ../qjsonitem.h \
    ../preferences/preferences.h
