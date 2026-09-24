#ifndef MANGAPLUS_H
#define MANGAPLUS_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class MangaPlus : public AbstractMangaSource
{
public:
    explicit MangaPlus(NetworkManager *dm);
    virtual ~MangaPlus() = default;

    bool updateMangaList(UpdateProgressToken *token) override;

    Result<QSharedPointer<MangaInfo>, QString> getMangaInfo(const QString &mangaUrl,
                                                            const QString &mangaTitle) override;
    void updateMangaInfoAsync(QSharedPointer<MangaInfo> info, bool updateCover = true) override;

    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;
    Result<QString, QString> getImageUrl(const QString &pageUrl) override;

private:
    struct RefreshedPageList
    {
        qint64 time = 0;
        QStringList imageUrls;
    };

    QString mangalistUrl;
    QString rankingUrl;
    QString titleDetailUrl;
    QString pagesUrl;

    QSet<QString> viewTokens;
    QHash<QString, RefreshedPageList> refreshedPageLists;

    QList<QWeakPointer<MangaInfo>> openedInfos;
    QObject replyContext;

    QString normalizeMangaUrl(const QString &url) const;
    QString normalizeChapterUrl(const QString &url) const;
    bool isUsableImageUrl(const QString &url) const;
    void cleanUpStoredUrls(QSharedPointer<MangaInfo> info);
    void trackInfo(QSharedPointer<MangaInfo> info);
    void forgetFailedImageUrl(QNetworkReply *reply);
};

#endif  // MANGAPLUS_H
