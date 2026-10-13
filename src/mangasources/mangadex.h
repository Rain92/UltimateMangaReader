#ifndef MANGADEX_H
#define MANGADEX_H

#include <QSet>
#include <algorithm>
#include <functional>
#include <tuple>

#include "abstractmangasource.h"
#include "mangainfo.h"

// What an update of the manga list got done so far, see UpdateProgressToken::resumeState().
struct MangaDexUpdateState
{
    MangaList mangas;
    QSet<QString> mangaIds;
    // -1: the popular titles, then the windows of the catalogue by creation date
    int window = -1;
    QString since;

    // of the window that is being crawled
    QMap<int, QList<QPair<QString, QString>>> pages;
    int total = 0;
    QString lastCreatedAt;

    int catalogueSize = 0;
    int processed = 0;
};

class MangaDex : public AbstractMangaSource
{
public:
    explicit MangaDex(NetworkManager *dm);
    virtual ~MangaDex() = default;

    bool updateMangaList(UpdateProgressToken *token) override;
    // the manga list has the urls of the mangas, the info also needs its authors and the cover
    QString mangaInfoRequestUrl(const QString &mangaUrl) const override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QList<ImageLink>, QString> getPageList(const QString &chapterUrl) override;

private:
    QString apiUrl;
    QString uploadsUrl;

    QElapsedTimer apiRequestTimer;
    int apiRequestIntervalMs;
    int apiRequestCount;
    QElapsedTimer atHomeClock;
    qint64 atHomeSchedule;

    Result<MangaChapterCollection, QString> getChapters(const QString &mangaId, bool resetOnNewVolume,
                                                        const QSet<QString> &loadedChapters);

    bool crawlMangaWindow(const QString &query, int maxResults, MangaDexUpdateState &state,
                          UpdateProgressToken *token);
    bool downloadThrottled(const QStringList &urls, UpdateProgressToken *token,
                           const std::function<bool(int, QSharedPointer<DownloadStringJob>)> &handler);
};

#endif  // MANGADEX_H
