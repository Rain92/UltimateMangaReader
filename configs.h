#ifndef CONFIGS_H
#define CONFIGS_H

#include <QRegExp>

#ifndef WINDOWS
#include "QtUtils.h"
#endif

#ifdef WINDOWS
const QString downloaddir = "D:/VMwareVMs/share/projects/testdownload";
const QString cachedir = "D:/VMwareVMs/share/projects/testdownload/cache";
#else
const QString downloaddir = "./ultimatemangareader";
const QString cachedir = downloaddir + "/cache";
#endif

const QString downloaddirimages = downloaddir + "/images";
const QString downloaddircovers = downloaddirimages + "/covers";
const QString manglistcachdir = cachedir + "/mangalists";
const QString readingstatesdir = cachedir + "/progress";

#ifdef WINDOWS

#define dpi 108
#define mm_to_px(mm) (mm * dpi * 0.0393701)

#else


#define dpi 300
#define nullptr NULL

#endif

#define mangasourceiconsize mm_to_px(15)
#define buttonsize mm_to_px(10)
#define buttonsizeaddfavorite mm_to_px(6)
#define scrollbarwidth mm_to_px(6)
#define scrollbarheight mm_to_px(5)
#define summaryscrollbarwidth mm_to_px(4)
#define listsourcessize mm_to_px(45)
#define coversize mm_to_px(50)
#define favoritesectonheight mm_to_px(20)
#define favoritesectionwidth mm_to_px(50)
#define favoritecoverheight mm_to_px(16)
#define favoritecoverwidth mm_to_px(16)

#define readerpreviouspagethreshold 0.3
#define readerbottommenuethreshold 0.9

#define imagecachesize 6


#define maxparalleldownloads 16


QString makePathLegal(QString filename);


#endif // CONFIGSS_H
