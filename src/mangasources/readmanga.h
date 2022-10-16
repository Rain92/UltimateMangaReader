#ifndef READMANGA_H
#define READMANGA_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class ReadManga : public AbstractMangaSource
{
public:
    explicit ReadManga(NetworkManager *dm);
    virtual ~ReadManga() = default;

    bool updateMangaList(UpdateProgressToken *token) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;

private:
    QString dictionaryUrl;
};

#endif  // READMANGA_H
