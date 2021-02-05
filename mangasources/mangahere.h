#ifndef MANGAHERE_H
#define MANGAHERE_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class MangaHere : public AbstractMangaSource
{
public:
    explicit MangaHere(NetworkManager *dm);
    virtual ~MangaHere() = default;

    bool updateMangaList(UpdateProgressToken *token) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;

private:
    QString dictionaryUrl;
};

#endif  // MANGAHERE_H
