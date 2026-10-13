#pragma once

#include <QSet>
#include <algorithm>
#include <functional>
#include <tuple>

#include "mangasourcebase.h"

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

class MangaDex : public MangaSourceBase
{
public:
    explicit MangaDex(NetworkManager *networkManager);

    Task<MangaListUpdate> fetchMangaList(UpdateProgressToken *token, MangaList current) override;
    Task<Result<MangaDetails>> fetchMangaInfo(QString mangaUrl, MangaChapterCollection known) override;
    Task<Result<QList<ImageLink>>> fetchPageList(QString chapterUrl) override;

private:
    QString apiUrl;
    QString uploadsUrl;

    QElapsedTimer apiRequestTimer;
    int apiRequestIntervalMs;
    int apiRequestCount;
    QElapsedTimer atHomeClock;
    qint64 atHomeSchedule;

    Task<Result<MangaChapterCollection>> fetchChapters(QString mangaId, bool resetOnNewVolume,
                                                       QSet<QString> loadedChapters);

    // the state is the one of the token
    Task<Result<>> crawlMangaWindow(QString query, int maxResults, MangaDexUpdateState *state,
                                    UpdateProgressToken *token);
    Task<Result<>> downloadThrottled(QStringList urls, UpdateProgressToken *token,
                                     std::function<bool(int, QSharedPointer<DownloadBufferJob>)> handler);

};
