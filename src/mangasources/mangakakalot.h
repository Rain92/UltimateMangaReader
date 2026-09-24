#ifndef MANGAKALOT_H
#define MANGAKALOT_H

#include <QSet>

#include "abstractmangasource.h"
#include "mangainfo.h"

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
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;

private:
    QString sitemapUrl;
    QString popularUrl;
    QString chaptersApiUrl;

    QHash<QString, QString> resolvedLegacyUrls;
    QHash<QString, QSharedPointer<DownloadStringJob>> prefetchedChapters;
    QSet<QString> imageHosts;

    void addImageHost(const QString &host);
    QString normalizeUrl(const QString &url) const;
    bool isMirrorUrl(const QString &url) const;
    Result<QString, QString> resolveMangaUrl(const QString &mangaUrl, const QString &mangaTitle);
    QSharedPointer<DownloadStringJob> downloadChapterList(const QString &slug, int offset);
};

#endif  // MANGAKALOT_H
