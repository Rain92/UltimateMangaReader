#ifndef MANGAPANDA_H
#define MANGAPANDA_H

#include "abstractmangasource.h"
#include "mangachapter.h"
#include "mangainfo.h"

class MangaPanda : public AbstractMangaSource
{
public:
    MangaPanda(QObject *parent, DownloadManager *dm);

    bool updateMangaList();
    QSharedPointer<MangaInfo> getMangaInfo(const QString &mangalink);
    QStringList getPageList(const QString &chapterlink);
    QString getImageLink(const QString &pagelink);

    void updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                        QSharedPointer<MangaInfo> info);
};

#endif  // MANGAPANDA_H
