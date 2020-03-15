
windows {
    DEFINES += DESKTOP WINDOWS
}

linux {
    DEFINES += DESKTOP LINUX
}

kobo {
    DEFINES += KOBO
    DEFINES -= DESKTOP

    target.path = /mnt/onboard/.adds/UltimateMangaReader
    INSTALLS += target
}

QT       += core gui widgets network virtualkeyboard

TARGET = UltimateMangaReader


INCLUDEPATH += $$PWD/widgets $$PWD/mangasources
DEPENDPATH  += $$PWD/widgets $$PWD/mangasources


RESOURCES += \
    images.qrc

FORMS += \
    widgets/favoriteswidget.ui \
    widgets/mainwidget.ui \
    widgets/homewidget.ui \
    widgets/mangainfowidget.ui \
    widgets/mangareaderwidget.ui \
    widgets/gotodialog.ui \
    widgets/updatedialog.ui \
    widgets/numpadwidget.ui \
    widgets/clearcachedialog.ui

HEADERS += \
    customgesturerecognizer.h \
    defines.h \
    downloadjobbase.h \
    mangalist.h \
    mangasources/abstractmangasource.h \
    mangasources/mangapanda.h \
#    mangasources/mangakakalot.h \
#    mangasources/mangadex.h \
    mangasources/jaiminisbox.h \
    utils.h \
    widgets/favoriteswidget.h \
    widgets/mainwidget.h \
    widgets/mangaimagecontainer.h \
    downloadfilejob.h \
    downloadimageandrescalejob.h \
    downloadmanager.h \
    downloadstringjob.h \
    mangachapter.h \
    mangaindex.h \
    mangainfo.h \
    preloadqueue.h \
    widgets/homewidget.h \
    widgets/mangainfowidget.h \
    widgets/mangareaderwidget.h \
    widgets/cscrollbar.h \
    favorite.h \
    favoritesmanager.h \
    widgets/gotodialog.h \
    widgets/updatedialog.h \
    settings.h \
    widgets/numpadwidget.h \
    widgets/clearcachedialog.h

SOURCES += \
    customgesturerecognizer.cpp \
    downloadjobbase.cpp \
    mangasources/abstractmangasource.cpp \
    mangasources/mangapanda.cpp \
#    mangasources/mangakakalot.cpp \
#    mangasources/mangadex.cpp \
    mangasources/jaiminisbox.cpp \
    utils.cpp \
    widgets/favoriteswidget.cpp \
    widgets/mainwidget.cpp \
    widgets/mangaimagecontainer.cpp \
    downloadfilejob.cpp \
    downloadimageandrescalejob.cpp \
    downloadmanager.cpp \
    downloadstringjob.cpp \
    main.cpp \
    mangachapter.cpp \
    mangaindex.cpp \
    mangainfo.cpp \
    preloadqueue.cpp \
    widgets/homewidget.cpp \
    widgets/mangainfowidget.cpp \
    widgets/mangareaderwidget.cpp \
    widgets/cscrollbar.cpp \
    favorite.cpp \
    favoritesmanager.cpp \
    widgets/gotodialog.cpp \
    widgets/updatedialog.cpp \
    settings.cpp \
    widgets/numpadwidget.cpp \
    widgets/clearcachedialog.cpp


