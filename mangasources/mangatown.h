#ifndef MANGATOWN_H
#define MANGATOWN_H

#include "abstractmangasource.h"
#include "mangachapter.h"
#include "mangainfo.h"

class MangaTown : public AbstractMangaSource
{
public:

    MangaTown(QObject *parent, DownloadManager *dm);

    bool updateMangaList();
    MangaInfo *getMangaInfo(QString mangalink);
    QStringList *getPageList(QString chapterlink);
    QString getImageLink(QString pagelink);

};
#endif // MANGATOWN_H
