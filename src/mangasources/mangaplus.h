#pragma once

#include "mangasourcebase.h"

class MangaPlus : public MangaSourceBase
{
public:
    explicit MangaPlus(NetworkManager *networkManager);

    Task<MangaListUpdate> fetchMangaList(UpdateProgressToken *token, MangaList current) override;
    Task<Result<MangaDetails>> fetchMangaInfo(QString mangaUrl, MangaChapterCollection known) override;
    Task<Result<QList<ImageLink>>> fetchPageList(QString chapterUrl) override;
    Task<Result<ImageLink>> resolveImage(ImageLink page) override;

private:
    struct RefreshedPageList
    {
        qint64 time = 0;
        QList<ImageLink> images;
    };

    QString mangalistUrl;
    QString rankingUrl;
    QString titleDetailUrl;
    QString pagesUrl;

    QSet<QByteArray> viewTokens;
    QHash<QString, RefreshedPageList> refreshedPageLists;
    // the refreshes that wait for their page list, by chapter
    QHash<QString, int> runningRefreshes;

    bool isUsableImage(const ImageLink &image) const;

};
