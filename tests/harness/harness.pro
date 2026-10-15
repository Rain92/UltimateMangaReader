# The test harness of the core, see tests/README.md. With CONFIG+=kobo it runs on the device (e.g. to time the
# replays), with the offscreen platform plugin.

TEMPLATE = app
TARGET = harness
QT += widgets
CONFIG += console
CONFIG -= app_bundle

include(../../src/core.pri)

# the sources of the harness unless --sources names other ones
DEFINES += HARNESS_SOURCES_DIR=\\\"$$clean_path($$PWD/../../sources)\\\"

SOURCES += \
    ../../src/device_desktop.cpp \
    bench.cpp \
    capture.cpp \
    fixtures.cpp \
    flowtests.cpp \
    indextool.cpp \
    jobtests.cpp \
    main.cpp \
    scripttests.cpp \
    storagetests.cpp \

HEADERS += \
    fixtures.h \
    harness.h \

kobo {
    LIBS += -lturbojpeg -lpng -lwebp -lrt
} else {
    CONFIG += link_pkgconfig
    PKGCONFIG += libturbojpeg libpng libwebp
}
