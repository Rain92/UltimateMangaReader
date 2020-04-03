#ifndef MANGAPANDA_H
#define MANGAPANDA_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class MangaPanda : public AbstractMangaSource
{
public:
    MangaPanda(QObject *parent, DownloadManager *dm);

    MangaList getMangaList() override;
    void updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job,
        QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(
        const QString &chapterlink) override;
    Result<QString, QString> getImageLink(const QString &pagelink) override;
};

#endif  // MANGAPANDA_H
