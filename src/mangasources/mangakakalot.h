#pragma once

#include <QElapsedTimer>
#include <QSet>

#include "mangasourcebase.h"

// What an update of the manga list got done so far, see UpdateProgressToken::resumeState().
struct MangakakalotUpdateState
{
    bool popularDone = false;
    // slug, title
    QList<QPair<QString, QString>> popular;
    // by sitemap url
    QHash<QString, QStringList> sitemapSlugs;
};

class Mangakakalot : public MangaSourceBase
{
public:
    explicit Mangakakalot(NetworkManager *networkManager);

    Task<MangaListUpdate> fetchMangaList(UpdateProgressToken *token, MangaList current) override;
    Task<Result<MangaDetails>> fetchMangaInfo(QString mangaUrl, MangaChapterCollection known) override;
    Task<Result<QList<ImageLink>>> fetchPageList(QString chapterUrl) override;

    // Which images of a chapter belong together (see joinSplitPages()): the indices of the pages' images, the
    // second one is -1 for a page that wasn't split. Invalid sizes are images that couldn't be loaded.
    static QList<QPair<int, int>> pairSplitImages(const QList<QSize> &sizes);

private:
    QString sitemapUrl;
    QString popularUrl;
    QString chaptersApiUrl;

    QSet<QString> imageHosts;

    void addImageHost(const QString &host);
    // Another url of the cover, for one that can't be downloaded (empty if there is none).
    QString coverFallbackUrl(const QString &coverUrl);
    Task<Result<QList<ImageLink>>> joinSplitPages(QStringList imageUrls);

    QString normalizeUrl(const QString &url) const;
    QSharedPointer<DownloadBufferJob> downloadChapterList(const QString &slug, int offset);
};
