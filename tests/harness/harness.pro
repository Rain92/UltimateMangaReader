# The test harness of the core (desktop only), see tests/README.md.

TEMPLATE = app
TARGET = harness
QT += widgets
CONFIG += console
CONFIG -= app_bundle

include(../../src/core.pri)

SOURCES += \
    ../../src/device_desktop.cpp \
    capture.cpp \
    fixtures.cpp \
    flowtests.cpp \
    jobtests.cpp \
    main.cpp \
    storagetests.cpp \

HEADERS += \
    fixtures.h \
    harness.h \

CONFIG += link_pkgconfig
PKGCONFIG += libturbojpeg libpng libwebp
