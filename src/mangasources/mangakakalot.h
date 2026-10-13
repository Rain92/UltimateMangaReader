#ifndef MANGAKAKALOT_H
#define MANGAKAKALOT_H

#include <QElapsedTimer>
#include <QSet>

#include "abstractmangasource.h"
#include "mangainfo.h"

// What an update of the manga list got done so far, see UpdateProgressToken::resumeState().
struct MangakakalotUpdateState
{
    bool popularDone = false;
    // slug, title
    QList<QPair<QString, QString>> popular;
    // by sitemap url
    QHash<QString, QStringList> sitemapSlugs;
};

class Mangakakalot : public AbstractMangaSource
{
public:
    Mangakakalot(NetworkManager *dm);
    virtual ~Mangakakalot() = default;

    bool updateMangaList(UpdateProgressToken *token) override;
    Result<QSharedPointer<MangaInfo>, QString> getMangaInfo(const QString &mangaUrl,
                                                            const QString &mangaTitle) override;
    void updateMangaInfoAsync(QSharedPointer<MangaInfo> info, bool updateCover = true) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QList<ImageLink>, QString> getPageList(const QString &chapterUrl) override;
    QString coverFallbackUrl(const QString &coverUrl) override;

    // Which images of a chapter belong together (see joinSplitPages()): the indices of the pages' images, the
    // second one is -1 for a page that wasn't split. Invalid sizes are images that couldn't be loaded.
    static QList<QPair<int, int>> pairSplitImages(const QList<QSize> &sizes);

private:
    QString sitemapUrl;
    QString popularUrl;
    QString chaptersApiUrl;

    // The first page of a chapter list, loaded while the manga page is: taken by updateMangaInfoFinishedLoading,
    // unless the manga page couldn't be loaded. Only used while it is fresh.
    struct PrefetchedChapters
    {
        QSharedPointer<DownloadStringJob> job;
        QElapsedTimer age;
    };
    QHash<QString, PrefetchedChapters> prefetchedChapters;
    void prefetchChapters(const QString &slug, QSharedPointer<DownloadStringJob> job);
    QSharedPointer<DownloadStringJob> takePrefetchedChapters(const QString &slug);
    QSet<QString> imageHosts;

    void addImageHost(const QString &host);
    Result<QList<ImageLink>, QString> joinSplitPages(const QStringList &imageUrls);
    QString normalizeUrl(const QString &url) const;
    QSharedPointer<DownloadStringJob> downloadChapterList(const QString &slug, int offset);
};

#endif  // MANGAKAKALOT_H
