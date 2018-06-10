#ifndef CONFIGS_H
#define CONFIGS_H

#include <QRegExp>

#ifndef WINDOWS
#include "QtUtils.h"
#endif


// dirstructure:
//               /cache/      -> favorites.dat
//               /cache/mangalists/ -> hostname_mangalist.dat
//               /cache/hostname/manganame/ -> mangainfo.dat progress.dat
//               /cache/hostname/manganame/images/ -> manganame_chapter_page.jpg/png

#ifdef WINDOWS

//#define cachedir QCoreApplication::applicationDirPath() + "/cache/"
//#define mangalistdir QCoreApplication::applicationDirPath() + "/cache/mangalists/"


#define cachedir "./UltimateMangaReader/cache/"
#define mangalistdir "./UltimateMangaReader/cache/mangalists/"

#else
#define cachedir "./UltimateMangaReader/cache/"
#define mangalistdir "./UltimateMangaReader/cache/mangalists/"
#endif

#define mangainfodir(hostname, manganame) (cachedir + hostname + "/" + makePathLegal(manganame) + "/")
#define mangaimagesdir(hostname, manganame) (mangainfodir(hostname, manganame) + "images/")


#ifdef WINDOWS

#define dpi 108
#define mm_to_px(mm) (mm * dpi * 0.0393701)

#else


#define dpi 300
#define nullptr NULL

#endif

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

#define maxparalleldownloads 16


QString makePathLegal(QString filename);


#endif // CONFIGSS_H
