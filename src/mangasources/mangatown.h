#ifndef MANGATOWN_H
#define MANGATOWN_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class MangaTown : public AbstractMangaSource
{
public:
    MangaTown(NetworkManager *dm);
    virtual ~MangaTown() = default;

    bool updateMangaList(UpdateProgressToken *token) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;
    Result<QString, QString> getImageUrl(const QString &pageUrl) override;

private:
    QString dictionaryUrl;
};

#endif  // MANGATOWN_H
