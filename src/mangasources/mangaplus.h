#pragma once

#include "abstractmangasource.h"
#include "mangainfo.h"

class MangaPlus : public AbstractMangaSource
{
public:
    explicit MangaPlus(NetworkManager *dm);
    virtual ~MangaPlus() = default;

    bool updateMangaList(UpdateProgressToken *token) override;

    void updateMangaInfoAsync(QSharedPointer<MangaInfo> info, bool updateCover = true) override;

    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadBufferJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QList<ImageLink>, QString> getPageList(const QString &chapterUrl) override;
    Result<ImageLink, QString> getImageLink(const ImageLink &page) override;

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

    QObject replyContext;

    bool isUsableImage(const ImageLink &image) const;
    void cleanUpStoredUrls(QSharedPointer<MangaInfo> info);
    void forgetFailedImageUrl(const QString &url);
};
