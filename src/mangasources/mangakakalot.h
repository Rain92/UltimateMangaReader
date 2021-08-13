#ifndef MANGAKALOT_H
#define MANGAKALOT_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class Mangakakalot : public AbstractMangaSource
{
public:
    Mangakakalot(NetworkManager *dm);
    virtual ~Mangakakalot() = default;

    bool updateMangaList(UpdateProgressToken *token) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;

private:
    QString dictionaryUrl;
};

#endif  // MANGAKALOT_H
