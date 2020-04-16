#ifndef MANGADEX_H
#define MANGADEX_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class MangaDex : public AbstractMangaSource
{
public:
    explicit MangaDex(DownloadManager *dm);

    bool uptareMangaList(UpdateProgressToken *token) override;
    void updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                        QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterlink) override;

private:
    void login();
};

#endif  // MANGADEX_H
