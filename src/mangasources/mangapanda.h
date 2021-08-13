#ifndef MANGAPANDA_H
#define MANGAPANDA_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class MangaPanda : public AbstractMangaSource
{
public:
    explicit MangaPanda(NetworkManager *dm);
    virtual ~MangaPanda() = default;

    bool updateMangaList(UpdateProgressToken *token) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;

private:
    QString dictionaryUrl;
};

#endif  // MANGAPANDA_H
