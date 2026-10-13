#ifndef COMICK_H
#define COMICK_H

#include <QSet>

#include "abstractmangasource.h"
#include "mangainfo.h"

// What an update of the manga list got done so far, see UpdateProgressToken::resumeState().
struct ComickUpdateState
{
    MangaList mangas;
    QSet<QString> urls;
    bool searched = false;
    QSet<int> sitemapsDone;
    // the titles of the list before the update by url: the cached infos and the reading progress are found by
    // title, it mustn't change (titles from the sitemaps are made from the slugs, the search has the real ones)
    QHash<QString, QString> knownTitles;
    bool knownTitlesLoaded = false;
};

// a request that gets an image into the cdn's cache, see comick.cpp
struct ComickCacheWarmUp;

class Comick : public AbstractMangaSource
{
public:
    explicit Comick(NetworkManager *dm);
    virtual ~Comick();

    bool updateMangaList(UpdateProgressToken *token) override;
    Result<QSharedPointer<MangaInfo>, QString> getMangaInfo(const QString &mangaUrl,
                                                            const QString &mangaTitle) override;
    void updateMangaInfoAsync(QSharedPointer<MangaInfo> info, bool updateCover = true) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;
    Result<QString, QString> getImageUrl(const QString &pageUrl) override;

private:
    QString apiUrl;
    QVector<QString> statuses;
    QHash<QString, QString> nextPages;
    QHash<QString, QSharedPointer<ComickCacheWarmUp>> pageWarmUps;

    QString normalizeUrl(const QString &url) const;
    bool fetchSearch(const QString &order, int pages, ComickUpdateState &state, UpdateProgressToken *token,
                     int progressFrom, int progressTo);
    bool fetchSitemaps(ComickUpdateState &state, UpdateProgressToken *token);
    Result<MangaChapterCollection, QString> fetchChapters(const QString &slug,
                                                          const MangaChapterCollection &current);
};

#endif  // COMICK_H
