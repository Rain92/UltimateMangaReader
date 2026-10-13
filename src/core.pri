# The core of the app: everything but the widgets, main.cpp and the device of the platform (device_*.cpp).
# Shared by the app and the test harness (tests/harness).

INCLUDEPATH += $$PWD $$PWD/mangasources $$PWD/scripting $$PWD/..

include($$PWD/../thirdparty/lua/lua.pri)

HEADERS += \
    $$PWD/aboutinfo.h \
    $$PWD/cachefile.h \
    $$PWD/device.h \
    $$PWD/downloadbufferjob.h \
    $$PWD/downloadfilejob.h \
    $$PWD/downloadimageandrescalejob.h \
    $$PWD/downloadjobbase.h \
    $$PWD/downloadqueue.h \
    $$PWD/enums.h \
    $$PWD/favorite.h \
    $$PWD/favoritesmanager.h \
    $$PWD/greyscaleimage.h \
    $$PWD/httpheader.h \
    $$PWD/imagelink.h \
    $$PWD/imageprocessingnative.h \
    $$PWD/imageprocessingqt.h \
    $$PWD/imageprocessor.h \
    $$PWD/imagerotate.h \
    $$PWD/mangachapter.h \
    $$PWD/mangachaptercollection.h \
    $$PWD/mangachapterdownloadmanager.h \
    $$PWD/mangacontroller.h \
    $$PWD/mangaindex.h \
    $$PWD/mangaindextraverser.h \
    $$PWD/mangainfo.h \
    $$PWD/mangalist.h \
    $$PWD/mangaliststore.h \
    $$PWD/mangalistupdater.h \
    $$PWD/mangarepository.h \
    $$PWD/mangasources/comick.h \
    $$PWD/mangasources/json.h \
    $$PWD/mangasources/luasource.h \
    $$PWD/mangasources/mangadex.h \
    $$PWD/mangasources/mangahere.h \
    $$PWD/mangasources/mangakakalot.h \
    $$PWD/mangasources/mangaplus.h \
    $$PWD/mangasources/mangasourcebase.h \
    $$PWD/mangasources/mangatown.h \
    $$PWD/mangasources/updateprogresstoken.h \
    $$PWD/networkmanager.h \
    $$PWD/perftimer.h \
    $$PWD/readingprogress.h \
    $$PWD/scripting/luaengine.h \
    $$PWD/scripting/lualibs.h \
    $$PWD/result.h \
    $$PWD/settings.h \
    $$PWD/sizes.h \
    $$PWD/sourcelistmodel.h \
    $$PWD/sourceregistry.h \
    $$PWD/stacktrace.h \
    $$PWD/standbymanager.h \
    $$PWD/staticsettings.h \
    $$PWD/suspendmanager.h \
    $$PWD/task.h \
    $$PWD/trimrect.h \
    $$PWD/ultimatemangareadercore.h \
    $$PWD/utils.h \
    $$PWD/../thirdparty/rapidjson.h \
    $$PWD/../thirdparty/simdimageresize.h \

SOURCES += \
    $$PWD/cachefile.cpp \
    $$PWD/downloadbufferjob.cpp \
    $$PWD/downloadfilejob.cpp \
    $$PWD/downloadimageandrescalejob.cpp \
    $$PWD/downloadjobbase.cpp \
    $$PWD/downloadqueue.cpp \
    $$PWD/favorite.cpp \
    $$PWD/favoritesmanager.cpp \
    $$PWD/greyscaleimage.cpp \
    $$PWD/imageprocessingnative.cpp \
    $$PWD/imageprocessingqt.cpp \
    $$PWD/imageprocessor.cpp \
    $$PWD/imagerotate.cpp \
    $$PWD/mangachapter.cpp \
    $$PWD/mangachaptercollection.cpp \
    $$PWD/mangachapterdownloadmanager.cpp \
    $$PWD/mangacontroller.cpp \
    $$PWD/mangaindex.cpp \
    $$PWD/mangaindextraverser.cpp \
    $$PWD/mangainfo.cpp \
    $$PWD/mangalist.cpp \
    $$PWD/mangaliststore.cpp \
    $$PWD/mangalistupdater.cpp \
    $$PWD/mangarepository.cpp \
    $$PWD/mangasources/comick.cpp \
    $$PWD/mangasources/luasource.cpp \
    $$PWD/mangasources/mangadex.cpp \
    $$PWD/mangasources/mangahere.cpp \
    $$PWD/mangasources/mangakakalot.cpp \
    $$PWD/mangasources/mangaplus.cpp \
    $$PWD/mangasources/mangasourcebase.cpp \
    $$PWD/mangasources/mangatown.cpp \
    $$PWD/mangasources/updateprogresstoken.cpp \
    $$PWD/networkmanager.cpp \
    $$PWD/readingprogress.cpp \
    $$PWD/scripting/luaengine.cpp \
    $$PWD/scripting/lualibs.cpp \
    $$PWD/settings.cpp \
    $$PWD/sourcelistmodel.cpp \
    $$PWD/sourceregistry.cpp \
    $$PWD/stacktrace.cpp \
    $$PWD/standbymanager.cpp \
    $$PWD/suspendmanager.cpp \
    $$PWD/trimrect.cpp \
    $$PWD/ultimatemangareadercore.cpp \
    $$PWD/utils.cpp \
    $$PWD/../thirdparty/simdimageresize.cpp \

QT += core gui network concurrent
CONFIG += c++23

# keep the code free of APIs that are deprecated in the Qt version it is built with
DEFINES += QT_DISABLE_DEPRECATED_UP_TO=0x060C00 QT_NO_FOREACH
