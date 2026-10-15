QT += core gui widgets

CONFIG += c++17
TEMPLATE = app
TARGET = mousejiggler

# Single source for the version: the APP_VERSION define used by the code, the
# Windows version resource, and the installer that CI builds from this file.
VERSION = 1.1.0
QMAKE_TARGET_COMPANY = Alejandro Exojo
QMAKE_TARGET_PRODUCT = Qt Mouse Jiggler
# FileDescription doubles as the process name Windows shows in the tray
# icon list and Task Manager — a friendly name, never prose.
QMAKE_TARGET_DESCRIPTION = Qt Mouse Jiggler
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

HEADERS += \
    src/constants.h \
    src/jiggler.h \
    src/mainwindow.h \
    src/platform.h

SOURCES += \
    src/app.cpp \
    src/jiggler.cpp \
    src/mainwindow.cpp \
    src/platform.cpp

win32 {
    RC_ICONS = src/mousejiggler.ico
    # Windows import libraries, explicit for every toolchain: MinGW links
    # user32/kernel32 implicitly, MSVC does not (LNK2019 __imp_SendInput and
    # friends otherwise). user32: SendInput, GetCursorPos. kernel32:
    # SetThreadExecutionState.
    LIBS += -luser32 -lkernel32
}

RESOURCES += src/resources.qrc

TRANSLATIONS += \
    src/l10n/es.ts \
    src/l10n/ca.ts

# Run `lrelease` as part of the build. So the TS files will be compiled to QM on
# regular builds. Also, the QM will be embedded into the app in the `:/i18n`
# directory (or whatever `QM_FILES_RESOURCE_PREFIX` is set to).
CONFIG += lrelease embed_translations

# Manual refresh of the translation source files, after changing any `tr()` or
# `translate()` string. Not part of the default build.
lupdate.target = lupdate
lupdate.commands = $$[QT_INSTALL_BINS]/lupdate $$PWD/mousejiggler.pro
QMAKE_EXTRA_TARGETS += lupdate
