
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
    mangasources/abstractmangasource.h \
    mangasources/mangapanda.h \
    mangasources/mangadex.h \
    mangasources/mangatown.h \
    widgets/favoriteswidget.h \
    widgets/mainwidget.h \
    widgets/mangaimagecontainer.h \
    configs.h \
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
    mangasources/jaiminisbox.h \
    mangasources/mangawindow.h \
    widgets/gotodialog.h \
    widgets/updatedialog.h \
    settings.h \
    widgets/numpadwidget.h \
    widgets/clearcachedialog.h

SOURCES += \
    customgesturerecognizer.cpp \
    mangasources/abstractmangasource.cpp \
    mangasources/mangapanda.cpp \
    mangasources/mangadex.cpp \
    mangasources/mangatown.cpp \
    widgets/favoriteswidget.cpp \
    widgets/mainwidget.cpp \
    widgets/mangaimagecontainer.cpp \
    configs.cpp \
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
    mangasources/jaiminisbox.cpp \
    mangasources/mangawindow.cpp \
    widgets/gotodialog.cpp \
    widgets/updatedialog.cpp \
    settings.cpp \
    widgets/numpadwidget.cpp \
    widgets/clearcachedialog.cpp


