QT += core gui widgets

CONFIG += c++17
TEMPLATE = app
TARGET = mousejiggler

# Single source for the version: the APP_VERSION define used by the code, the
# Windows version resource, and the installer that CI builds from this file.
VERSION = 1.1.1
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

win32 {
    # Deploy the Qt runtime next to the executable, for manual testing of a
    # release-like directory (what CI does with its own explicit command).
    # Paths come from the kit in use ($$[QT_INSTALL_BINS]), so nothing is
    # hardcoded, and the target works from any build directory.
    # The executable path is composed by hand instead of using the make-level
    # $(DESTDIR_TARGET): that variable is only defined in the per-config
    # makefiles (Makefile.Debug/Makefile.Release), not in the top-level
    # Makefile where QMAKE_EXTRA_TARGETS rules are emitted, so it would
    # expand to nothing exactly when the target runs.
    CONFIG(debug, debug|release) {
        deployment_config = --debug
        deployment_dir = debug
    } else {
        deployment_config = --release
        deployment_dir = release
    }
    debug_and_release {
        # The executable lands in the per-config subdirectory.
        deployment_target = $$clean_path($$OUT_PWD/$$deployment_dir/$${TARGET}.exe)
    } else {
        deployment_target = $$clean_path($$OUT_PWD/$$DESTDIR/$${TARGET}.exe)
    }
    windeployqt.target = windeployqt
    windeployqt.depends = first
    windeployqt.commands = $$shell_quote($$shell_path($$[QT_INSTALL_BINS]/windeployqt.exe)) \
        $$deployment_config --no-translations $$shell_quote($$shell_path($$deployment_target))
    QMAKE_EXTRA_TARGETS += windeployqt
}
