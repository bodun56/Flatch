QT += core gui widgets sql network

TEMPLATE = app
TARGET = Flatch
VERSION = 1.0.0

CONFIG += c++17 warn_on
CONFIG -= app_bundle

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += FLATCH_VERSION=\\\"$$VERSION\\\"

win32: LIBS += -lcrypt32
mingw: QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8

QMAKE_TARGET_PRODUCT = Flatch
QMAKE_TARGET_DESCRIPTION = "Flatch - snippet manager for Windows and Linux"
QMAKE_TARGET_COMPANY = BoDun
QMAKE_TARGET_COPYRIGHT = "Copyright (C) 2026 BoDun"

SOURCES += \
    src/main.cpp \
    src/aboutdialog.cpp \
    src/appcontroller.cpp \
    src/apppaths.cpp \
    src/autostart.cpp \
    src/crypto.cpp \
    src/database.cpp \
    src/preferences.cpp \
    src/settingsdialog.cpp \
    src/snippet.cpp \
    src/snippetactions.cpp \
    src/snippetdialog.cpp \
    src/traycontroller.cpp

HEADERS += \
    src/aboutdialog.h \
    src/appcontroller.h \
    src/appicon.h \
    src/apppaths.h \
    src/autostart.h \
    src/crypto.h \
    src/database.h \
    src/preferences.h \
    src/settingsdialog.h \
    src/snippet.h \
    src/snippetactions.h \
    src/snippetdialog.h \
    src/traycontroller.h \
    src/uiutils.h

RESOURCES += resources/resources.qrc

RC_ICONS = resources/icons/flatch.ico
