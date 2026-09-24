#ifndef MANGADEX_H
#define MANGADEX_H

#include <QQueue>
#include <QSet>
#include <algorithm>
#include <functional>
#include <tuple>

#include "abstractmangasource.h"
#include "mangainfo.h"

#define RAPIDJSON_ASSERT(x) \
    if (!(x))               \
        throw QException();

#include "thirdparty/rapidjson.h"

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

    bool crawlMangaWindow(const QString &query, int maxResults, MangaList &mangas, QSet<QString> &mangaIds,
                          UpdateProgressToken *token, int &total, QString &lastCreatedAt,
                          const std::function<void(int)> &progress);
    bool downloadThrottled(const QStringList &urls, UpdateProgressToken *token,
                           const std::function<bool(int, QSharedPointer<DownloadStringJob>)> &handler);
};

#endif  // MANGADEX_H
