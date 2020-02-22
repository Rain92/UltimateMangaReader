#ifndef CONFIGS_H
#define CONFIGS_H

#include <QRegExp>

// dirstructure:
//               /cache/      -> favorites.dat
//               /cache/mangalists/ -> hostname_mangalist.dat
//               /cache/hostname/manganame/ -> mangainfo.dat progress.dat
//               /cache/hostname/manganame/images/ ->
//               manganame_chapter_page.jpg/png

#define cachedir QCoreApplication::applicationDirPath() + "/cache/"
#define mangalistdir cachedir + "mangalists/"

#define mangainfodir(hostname, manganame) \
    (cachedir + hostname + "/" + makePathLegal(manganame) + "/")
#define mangaimagesdir(hostname, manganame) \
    (mangainfodir(hostname, manganame) + "images/")

#ifdef DESKTOP
#define screen_dpi 108
#else
#define screen_dpi 300
#endif

#define mm_to_px(mm) (mm * screen_dpi * 0.0393701)

#define listsourceswidth mm_to_px(65)
#define listsourcesheight mm_to_px(50)
#define mangasourceiconsize mm_to_px(14)
#define mangasourceitemwidth mm_to_px(21)
#define mangasourceitemheight mm_to_px(20)
#define mangacourceiconspacing mm_to_px(3)

#define buttonsize mm_to_px(10)
#define buttonsizeaddfavorite mm_to_px(8)

#define resourceiconsize mm_to_px(6)
#define batteryiconsize mm_to_px(3)

#define scrollbarsliderminlength mm_to_px(8)
#define scrollbarwidth mm_to_px(6)
#define scrollbarheight mm_to_px(4)
#define summaryscrollbarwidth mm_to_px(5)

#define coversize mm_to_px(50)

#define favoritesectonheight mm_to_px(20)
#define favoritesectionwidth mm_to_px(50)
#define favoritecoverheight mm_to_px(16)
#define favoritecoverwidth mm_to_px(16)

#define readerpreviouspagethreshold 0.3
#define readerbottommenuethreshold 0.1

#define frontlightslidergrooveheight mm_to_px(5)
#define frontlightsliderhandlewidth mm_to_px(7)
#define frontlightsliderhandleheight mm_to_px(6)

#define imagecachesize 6

#define maxparalleldownloads 10

QString makePathLegal(QString filename);

#endif  // CONFIGSS_H
