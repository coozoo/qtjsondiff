#-------------------------------------------------
#
# Project created by QtCreator 2017-04-27T12:05:13
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qtjsondiff
TRANSLATIONS = ./translations/$${TARGET}_en_US.ts ./translations/$${TARGET}_uk_UA.ts
TEMPLATE = app

CONFIG += c++17
CONFIG += openssl-linked
CONFIG += lrelease
# do not show qDebug() messages in release builds
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT

QMAKE_CXXFLAGS += -Wno-implicit-fallthrough

SOURCES += main.cpp\
    commandlineparser.cpp \
    jsonitemdelegate.cpp \
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
    jsonitemdelegate.h \
    jsonsyntaxhighlighter.h \
    preferences/preferences.h \
    preferences/preferencesdialog.h \
    preferences/shortcutdelegate.h \
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
icon.files += images/qtjsondiff.png
icon.path += /usr/share/icons/hicolor/scalable/apps
desktop.files += $${TARGET}.desktop
desktop.path += /usr/share/applications/
INSTALLS += binary translations icon desktop

# Tests are opt-in: pass `CONFIG+=tests` to qmake to add the `check`
# target. Release builds (build_all.yml) never set it, so the generated
# Makefile has no check rule and no tests/-related references at all.
#
# Tests build into a shadow subdir under the current Makefile dir
# ($$OUT_PWD). For a shadow build like build/Desktopqt6-Debug that puts
# them under build/Desktopqt6-Debug/tests-shadow. Source path comes from
# $$PWD (the directory of this .pro file).
tests {
    check.commands = \
        mkdir -p $$OUT_PWD/tests-shadow && \
        cd $$OUT_PWD/tests-shadow && \
        $(QMAKE) $$PWD/tests/tests.pro && \
        $(MAKE) && \
        QT_QPA_PLATFORM=offscreen ./conversions/tst_json_conversions && \
        QT_QPA_PLATFORM=offscreen ./compare/tst_compare

    QMAKE_EXTRA_TARGETS += check
}
