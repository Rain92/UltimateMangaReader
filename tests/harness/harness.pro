# The test harness of the core, see tests/README.md. With CONFIG+=kobo it runs on the device (e.g. to time the
# replays), with the offscreen platform plugin.

TEMPLATE = app
TARGET = harness
QT += widgets
CONFIG += console
CONFIG -= app_bundle

include(../../src/core.pri)

# the C++ sources, the reference for the scripts (not in the app)
HEADERS += \
    ../../src/mangasources/comick.h \
    ../../src/mangasources/mangadex.h \
    ../../src/mangasources/mangahere.h \
    ../../src/mangasources/mangakakalot.h \
    ../../src/mangasources/mangaplus.h \
    ../../src/mangasources/mangatown.h \

SOURCES += \
    ../../src/mangasources/comick.cpp \
    ../../src/mangasources/mangadex.cpp \
    ../../src/mangasources/mangahere.cpp \
    ../../src/mangasources/mangakakalot.cpp \
    ../../src/mangasources/mangaplus.cpp \
    ../../src/mangasources/mangatown.cpp \

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
