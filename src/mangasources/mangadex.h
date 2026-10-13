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
    Result<QSharedPointer<MangaInfo>, QString> getMangaInfo(const QString &mangaUrl,
                                                            const QString &mangaTitle) override;
    void updateMangaInfoAsync(QSharedPointer<MangaInfo> info, bool updateCover = true) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;

private:
    QString apiUrl;
    QString uploadsUrl;

    QElapsedTimer apiRequestTimer;
    int apiRequestIntervalMs;
    int apiRequestCount;
    QElapsedTimer atHomeClock;
    qint64 atHomeSchedule;

    QString mangaRequestUrl(const QString &mangaUrl) const;
    Result<MangaChapterCollection, QString> getChapters(const QString &mangaId, bool resetOnNewVolume,
                                                        const QSet<QString> &loadedChapters);

    bool crawlMangaWindow(const QString &query, int maxResults, MangaDexUpdateState &state,
                          UpdateProgressToken *token);
    bool downloadThrottled(const QStringList &urls, UpdateProgressToken *token,
                           const std::function<bool(int, QSharedPointer<DownloadStringJob>)> &handler);
};

#endif  // MANGADEX_H
