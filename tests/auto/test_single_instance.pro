TEMPLATE = app
TARGET = test_single_instance
CONFIG += console testcase
QT = core testlib

# The test spawns the real application and links against none of its code.
# Build it from a directory inside the application build directory, so that
# the default path to the application binary resolves:
#     mkdir -p build/tests && cd build/tests
#     qmake ../../tests/auto/test_single_instance.pro && make check
#
# On Windows the application keeps the qmake default debug_and_release
# layout (packaging expects build\release\mousejiggler.exe), so its binary
# sits one directory deeper than on Unix. The test itself opts out of that
# layout: a single flat configuration keeps "nmake check" from building and
# running debug and release variants.
win32 {
    CONFIG -= debug_and_release debug_and_release_target
    CONFIG += release
    applicationBinary = $$clean_path($$OUT_PWD/../release/mousejiggler.exe)
} else {
    applicationBinary = $$clean_path($$OUT_PWD/../mousejiggler)
}
DEFINES += APPLICATION_BINARY_PATH=\\\"$$applicationBinary\\\"

SOURCES += test_single_instance.cpp
