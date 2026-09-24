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
    Result<QString, QString> getImageUrl(const QString &pageUrl) override;

private:
    QString mobileUrl;
    QString fanfoxUrl;
    QString dictionaryUrl;
    QString mangaListUrl;

    Result<QStringList, QString> getPageListDesktop(const QString &chapterUrl, int timeout);
};

#endif  // MANGAHERE_H
