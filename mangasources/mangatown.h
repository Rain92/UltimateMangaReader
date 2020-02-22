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
    QSharedPointer<MangaInfo> getMangaInfo(const QString &mangalink);
    void updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                        QSharedPointer<MangaInfo> info);
    QStringList getPageList(const QString &chapterlink);
    QString getImageLink(const QString &pagelink);
};
#endif  // MANGATOWN_H
