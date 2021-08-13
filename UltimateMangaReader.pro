
kobo {
    DEFINES  += KOBO
    DEFINES  += __ARM_NEON__
    DEFINES  -= DESKTOP

    KOBO_PLATFORM_PLUGIN_PATH = $$PWD/../qt5-kobo-platform-plugin

    INCLUDEPATH += $$KOBO_PLATFORM_PLUGIN_PATH/src

    target.path = /mnt/onboard/.adds/UltimateMangaReader
    INSTALLS += target
}
else { # default is desktop
    DEFINES  += DESKTOP
}

QT    += core gui widgets network svg
CONFIG += c++17
QMAKE_LFLAGS += -rdynamic

LIBS +=  -lturbojpeg -lpng

TARGET = UltimateMangaReader

INCLUDEPATH += $$PWD/src $$PWD/src/widgets $$PWD/src/mangasources

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
    src/aboutinfo.h \
    src/dither.h \
    src/downloadbufferjob.h \
    src/downloadfilejob.h \
    src/downloadimageandrescalejob.h \
    src/downloadimagedescriptor.h \
    src/downloadjobbase.h \
    src/downloadqueue.h \
    src/downloadstringjob.h \
    src/enums.h \
    src/favorite.h \
    src/favoritesmanager.h \
    src/greyscaleimage.h \
    src/imageprocessingnative.h \
    src/imageprocessingqt.h \
    src/imagerotate.h \
    src/mangachapter.h \
    src/mangachaptercollection.h \
    src/mangachapterdownloadmanager.h \
    src/mangacontroller.h \
    src/mangaindex.h \
    src/mangaindextraverser.h \
    src/mangainfo.h \
    src/mangalist.h \
    src/mangasources/abstractmangasource.h \
    src/mangasources/mangadex.h \
    src/mangasources/mangago.h \
    src/mangasources/mangahere.h \
    src/mangasources/mangahub.h \
    src/mangasources/mangakakalot.h \
    src/mangasources/mangaowl.h \
    src/mangasources/mangapanda.h \
    src/mangasources/mangaplus.h \
    src/mangasources/mangatown.h \
    src/mangasources/updateprogresstoken.h \
    src/networkmanager.h \
    src/readingprogress.h \
    src/settings.h \
    src/sizes.h \
    src/stacktrace.h \
    src/staticsettings.h \
    src/suspendmanager.h \
    src/ultimatemangareadercore.h \
    src/utils.h \
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
    src/widgets/updatemangalistsdialog.h \
    src/widgets/virtualkeyboard.h \
    src/widgets/wifidialog.h \
    thirdparty/picoproto.h \
    thirdparty/rapidjson.h \
    thirdparty/result.h \
    thirdparty/simdimageresize.h \


SOURCES += \
    src/dither.cpp \
    src/downloadbufferjob.cpp \
    src/downloadfilejob.cpp \
    src/downloadimageandrescalejob.cpp \
    src/downloadjobbase.cpp \
    src/downloadqueue.cpp \
    src/downloadstringjob.cpp \
    src/favorite.cpp \
    src/favoritesmanager.cpp \
    src/greyscaleimage.cpp \
    src/imageprocessingnative.cpp \
    src/imageprocessingqt.cpp \
    src/imagerotate.cpp \
    src/main.cpp \
    src/mangachapter.cpp \
    src/mangachaptercollection.cpp \
    src/mangachapterdownloadmanager.cpp \
    src/mangacontroller.cpp \
    src/mangaindex.cpp \
    src/mangaindextraverser.cpp \
    src/mangainfo.cpp \
    src/mangalist.cpp \
    src/mangasources/abstractmangasource.cpp \
    src/mangasources/mangadex.cpp \
    src/mangasources/mangago.cpp \
    src/mangasources/mangahere.cpp \
    src/mangasources/mangahub.cpp \
    src/mangasources/mangakakalot.cpp \
    src/mangasources/mangaowl.cpp \
    src/mangasources/mangapanda.cpp \
    src/mangasources/mangaplus.cpp \
    src/mangasources/mangatown.cpp \
    src/mangasources/updateprogresstoken.cpp \
    src/networkmanager.cpp \
    src/readingprogress.cpp \
    src/settings.cpp \
    src/suspendmanager.cpp \
    src/ultimatemangareadercore.cpp \
    src/utils.cpp \
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
    src/widgets/updatemangalistsdialog.cpp \
    src/widgets/virtualkeyboard.cpp \
    src/widgets/wifidialog.cpp \
    thirdparty/picoproto.cc \
    thirdparty/simdimageresize.cpp \

