#ifndef MANGAKALOT_H
#define MANGAKALOT_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class Mangakakalot : public AbstractMangaSource
{
public:
    Mangakakalot(QObject *parent, DownloadManager *dm);

    MangaList getMangaList() override;
    void updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job,
        QSharedPointer<MangaInfo> info) override;
    QStringList getPageList(const QString &chapterlink) override;
    QString getImageLink(const QString &pagelink) override;

private:
    QString dicturl;
};

#endif  // MANGAKALOT_H
