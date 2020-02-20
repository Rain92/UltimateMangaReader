#ifndef MANGAWINDOW_H
#define MANGAWINDOW_H

#include "abstractmangasource.h"
#include "mangachapter.h"
#include "mangainfo.h"

class MangaWindow : public AbstractMangaSource
{
public:
    MangaWindow(QObject *parent, DownloadManager *dm);

    bool updateMangaList();
    MangaInfo *getMangaInfo(QString mangalink);
    void updateMangaInfoFinishedLoading(DownloadStringJob *job,
                                        MangaInfo *info);
    QStringList getPageList(const QString &chapterlink);
    QString getImageLink(const QString &pagelink);
};

#endif  // MANGAWINDOW_H
