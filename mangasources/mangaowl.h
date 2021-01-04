#ifndef MANGAOWL_H
#define MANGAOWL_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class MangaOwl : public AbstractMangaSource
{
public:
    explicit MangaOwl(NetworkManager *dm);

    bool updateMangaList(UpdateProgressToken *token) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;

private:
    QString mangalistUrl;
};

#endif  // MANGAOWL_H
