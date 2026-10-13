#pragma once

#include <QSet>

#include "mangasourcebase.h"

// What an update of the manga list got done so far, see UpdateProgressToken::resumeState().
struct ComickUpdateState
{
    MangaList mangas;
    QSet<QString> urls;
    bool searched = false;
    QSet<int> sitemapsDone;
    // the titles of the list before the update by url: the sitemaps have no titles (only slugs), the ones the
    // search found before are kept
    QHash<QString, QString> knownTitles;
    bool knownTitlesLoaded = false;
};

class Comick : public MangaSourceBase
{
public:
    explicit Comick(NetworkManager *networkManager);
    virtual ~Comick();

    Task<MangaListUpdate> fetchMangaList(UpdateProgressToken *token, MangaList current) override;
    Task<Result<MangaDetails>> fetchMangaInfo(QString mangaUrl, MangaChapterCollection known) override;
    Task<Result<QList<ImageLink>>> fetchPageList(QString chapterUrl) override;
    Task<Result<ImageLink>> resolveImage(ImageLink page) override;

private:
    QString apiUrl;
    QVector<QString> statuses;
    QHash<QString, QString> nextPages;
    // requests that get the next pages into the cdn's cache
    QHash<QString, QSharedPointer<DownloadBufferJob>> pageWarmUps;

    QSharedPointer<DownloadBufferJob> startCacheWarmUp(const QString &imageUrl, int timeout);

    // the state is the one of the token
    Task<bool> fetchSearch(QString order, int pages, ComickUpdateState *state, UpdateProgressToken *token,
                           int progressFrom, int progressTo);
    Task<Result<>> fetchSitemaps(ComickUpdateState *state, UpdateProgressToken *token);
    Task<Result<MangaChapterCollection>> fetchChapters(QString slug, MangaChapterCollection current);

};
