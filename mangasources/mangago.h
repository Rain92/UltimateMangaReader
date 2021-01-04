#ifndef MANGAGO_H
#define MANGAGO_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class MangaGo : public AbstractMangaSource
{
public:
    explicit MangaGo(NetworkManager *dm);

    bool updateMangaList(UpdateProgressToken *token) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;

private:
    QString mangalistUrl;
};

#endif  // MANGAGO_H
