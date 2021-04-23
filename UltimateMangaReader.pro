
kobo {
    DEFINES  += KOBO
    DEFINES  += __ARM_NEON__
    DEFINES  -= DESKTOP

    KOBO_PLATFORM_PLUGIN_PATH = $$PWD/../qt5-kobo-platform-plugin

    INCLUDEPATH += $$KOBO_PLATFORM_PLUGIN_PATH

    target.path = /mnt/onboard/.adds/UltimateMangaReader
    INSTALLS += target
}
else { # default is desktop
    DEFINES  += DESKTOP
}

QT    += core gui widgets network svg
CONFIG += c++17

LIBS +=  -lturbojpeg -lpng

TARGET = UltimateMangaReader

INCLUDEPATH += $$PWD/widgets $$PWD/mangasources

RESOURCES += \
    resources.qrc

FORMS += \
    widgets/downloadmangachaptersdialog.ui \
    widgets/downloadstatusdialog.ui \
    widgets/errormessagewidget.ui \
    widgets/favoriteswidget.ui \
    widgets/mainwidget.ui \
    widgets/homewidget.ui \
    widgets/mangainfowidget.ui \
    widgets/mangareaderwidget.ui \
    widgets/gotodialog.ui \
    widgets/menudialog.ui \
    widgets/screensaverdialog.ui \
    widgets/settingsdialog.ui \
    widgets/numpadwidget.ui \
    widgets/clearcachedialog.ui \
    widgets/updatemangalistsdialog.ui \
    widgets/wifidialog.ui

HEADERS += \
    dither.h \
    downloadbufferjob.h \
    enums.h \
    greyscaleimage.h \
    imageprocessingnative.h \
    imageprocessingqt.h \
    imagerotate.h \
    mangachaptercollection.h \
    mangachapterdownloadmanager.h \
    mangacontroller.h \
    mangaindextraverser.h \
    mangasources/mangago.h \
    mangasources/mangahere.h \
    mangasources/mangakakalot.h \
    mangasources/mangaplus.h \
    mangasources/mangatown.h \
    mangasources/updateprogresstoken.h \
    networkmanager.h \
    readingprogress.h \
    sizes.h \
    staticsettings.h \
    suspendmanager.h \
    thirdparty/picoproto.h \
    thirdparty/rapidjson.h \
    thirdparty/result.h \
    thirdparty/simdimageresize.h \
    ultimatemangareadercore.h \
    widgets/batteryicon.h \
    widgets/customgesturerecognizer.h \
    downloadimagedescriptor.h \
    downloadjobbase.h \
    downloadqueue.h \
    mangalist.h \
    mangasources/abstractmangasource.h \
    mangasources/mangahub.h \
    mangasources/mangaowl.h \
    mangasources/mangapanda.h \
    mangasources/mangadex.h \
    utils.h \
    widgets/clineedit.h \
    widgets/downloadmangachaptersdialog.h \
    widgets/downloadstatusdialog.h \
    widgets/errormessagewidget.h \
    widgets/favoriteswidget.h \
    widgets/mainwidget.h \
    downloadfilejob.h \
    downloadimageandrescalejob.h \
    downloadstringjob.h \
    mangachapter.h \
    mangaindex.h \
    mangainfo.h \
    widgets/homewidget.h \
    widgets/mangaimagewidget.h \
    widgets/mangainfowidget.h \
    widgets/mangareaderwidget.h \
    favorite.h \
    favoritesmanager.h \
    widgets/gotodialog.h \
    widgets/menudialog.h \
    widgets/screensaverdialog.h \
    widgets/settingsdialog.h \
    settings.h \
    widgets/numpadwidget.h \
    widgets/clearcachedialog.h \
    widgets/updatemangalistsdialog.h \
    widgets/virtualkeyboard.h \
    widgets/wifidialog.h

SOURCES += \
    dither.cpp \
    downloadbufferjob.cpp \
    greyscaleimage.cpp \
    imageprocessingnative.cpp \
    imageprocessingqt.cpp \
    imagerotate.cpp \
    mangachaptercollection.cpp \
    mangachapterdownloadmanager.cpp \
    mangacontroller.cpp \
    mangaindextraverser.cpp \
    mangalist.cpp \
    mangasources/mangago.cpp \
    mangasources/mangahere.cpp \
    mangasources/mangakakalot.cpp \
    mangasources/mangaplus.cpp \
    mangasources/mangatown.cpp \
    mangasources/updateprogresstoken.cpp \
    networkmanager.cpp \
    readingprogress.cpp \
    suspendmanager.cpp \
    thirdparty/picoproto.cc \
    thirdparty/simdimageresize.cpp \
    ultimatemangareadercore.cpp \
    widgets/batteryicon.cpp \
    widgets/customgesturerecognizer.cpp \
    downloadjobbase.cpp \
    downloadqueue.cpp \
    mangasources/abstractmangasource.cpp \
    mangasources/mangahub.cpp \
    mangasources/mangaowl.cpp \
    mangasources/mangapanda.cpp \
    mangasources/mangadex.cpp \
    utils.cpp \
    widgets/clineedit.cpp \
    widgets/downloadmangachaptersdialog.cpp \
    widgets/downloadstatusdialog.cpp \
    widgets/errormessagewidget.cpp \
    widgets/favoriteswidget.cpp \
    widgets/mainwidget.cpp \
    downloadfilejob.cpp \
    downloadimageandrescalejob.cpp \
    downloadstringjob.cpp \
    main.cpp \
    mangachapter.cpp \
    mangaindex.cpp \
    mangainfo.cpp \
    widgets/homewidget.cpp \
    widgets/mangaimagewidget.cpp \
    widgets/mangainfowidget.cpp \
    widgets/mangareaderwidget.cpp \
    favorite.cpp \
    favoritesmanager.cpp \
    widgets/gotodialog.cpp \
    widgets/menudialog.cpp \
    widgets/screensaverdialog.cpp \
    widgets/settingsdialog.cpp \
    settings.cpp \
    widgets/numpadwidget.cpp \
    widgets/clearcachedialog.cpp \
    widgets/updatemangalistsdialog.cpp \
    widgets/virtualkeyboard.cpp \
    widgets/wifidialog.cpp


