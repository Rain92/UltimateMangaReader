
kobo {
    # the platform plugin's source (koboplatformfunctions.h), by default next to the app's,
    # e.g. qmake KOBO_PLATFORM_PLUGIN_PATH=/path/to/qt6-kobo-platform-plugin
    isEmpty(KOBO_PLATFORM_PLUGIN_PATH): KOBO_PLATFORM_PLUGIN_PATH = $$PWD/../qt6-kobo-platform-plugin

    INCLUDEPATH += $$KOBO_PLATFORM_PLUGIN_PATH/src

    # the libraries are in the toolchain's sysroot
    LIBS += -lturbojpeg -lpng -lwebp

    # clock_gettime lives in librt in the Kobo toolchain's glibc 2.15
    LIBS += -lrt

    SOURCES += src/device_kobo.cpp

    target.path = /mnt/onboard/.adds/UltimateMangaReader
    INSTALLS += target
}
else { # default is desktop
    # the native image pipeline uses the tj3 api of libjpeg-turbo 3.0+
    CONFIG += link_pkgconfig
    PKGCONFIG += libturbojpeg libpng libwebp

    SOURCES += src/device_desktop.cpp
}

QT    += widgets svg
# backtraces with function names (stacktrace.cpp)
QMAKE_LFLAGS += -rdynamic

TARGET = UltimateMangaReader

include(src/core.pri)

INCLUDEPATH += $$PWD/src/widgets

RESOURCES += \
    resources.qrc

FORMS += \
    src/widgets/aboutdialog.ui \
    src/widgets/clearcachedialog.ui \
    src/widgets/downloadmangachaptersdialog.ui \
    src/widgets/downloadstatusdialog.ui \
    src/widgets/errormessagewidget.ui \
    src/widgets/favoriteswidget.ui \
    src/widgets/gotodialog.ui \
    src/widgets/homewidget.ui \
    src/widgets/mainwidget.ui \
    src/widgets/mangainfowidget.ui \
    src/widgets/mangareaderwidget.ui \
    src/widgets/menudialog.ui \
    src/widgets/numpadwidget.ui \
    src/widgets/screensaverdialog.ui \
    src/widgets/settingsdialog.ui \
    src/widgets/updatemangalistsdialog.ui \
    src/widgets/wifidialog.ui \


HEADERS += \
    src/widgets/aboutdialog.h \
    src/widgets/batteryicon.h \
    src/widgets/clearcachedialog.h \
    src/widgets/clineedit.h \
    src/widgets/customgesturerecognizer.h \
    src/widgets/downloadmangachaptersdialog.h \
    src/widgets/downloadstatusdialog.h \
    src/widgets/errormessagewidget.h \
    src/widgets/favoriteswidget.h \
    src/widgets/gotodialog.h \
    src/widgets/homewidget.h \
    src/widgets/mainwidget.h \
    src/widgets/mangaimagewidget.h \
    src/widgets/mangainfowidget.h \
    src/widgets/mangareaderwidget.h \
    src/widgets/menudialog.h \
    src/widgets/numpadwidget.h \
    src/widgets/screensaverdialog.h \
    src/widgets/settingsdialog.h \
    src/widgets/sourcerepositorydialog.h \
    src/widgets/sourceupdatedialog.h \
    src/widgets/sourcechecklist.h \
    src/widgets/updatemangalistsdialog.h \
    src/widgets/virtualkeyboard.h \
    src/widgets/widgetutils.h \
    src/widgets/wifidialog.h \


SOURCES += \
    src/main.cpp \
    src/widgets/aboutdialog.cpp \
    src/widgets/batteryicon.cpp \
    src/widgets/clearcachedialog.cpp \
    src/widgets/clineedit.cpp \
    src/widgets/customgesturerecognizer.cpp \
    src/widgets/downloadmangachaptersdialog.cpp \
    src/widgets/downloadstatusdialog.cpp \
    src/widgets/errormessagewidget.cpp \
    src/widgets/favoriteswidget.cpp \
    src/widgets/gotodialog.cpp \
    src/widgets/homewidget.cpp \
    src/widgets/mainwidget.cpp \
    src/widgets/mangaimagewidget.cpp \
    src/widgets/mangainfowidget.cpp \
    src/widgets/mangareaderwidget.cpp \
    src/widgets/menudialog.cpp \
    src/widgets/numpadwidget.cpp \
    src/widgets/screensaverdialog.cpp \
    src/widgets/settingsdialog.cpp \
    src/widgets/sourcerepositorydialog.cpp \
    src/widgets/sourceupdatedialog.cpp \
    src/widgets/sourcechecklist.cpp \
    src/widgets/updatemangalistsdialog.cpp \
    src/widgets/virtualkeyboard.cpp \
    src/widgets/widgetutils.cpp \
    src/widgets/wifidialog.cpp \

