#pragma once

#include <map>

#include "downloadqueue.h"
#include "mangainfo.h"
#include "result.h"
#include "sourceregistry.h"
#include "task.h"

// What the app knows about the mangas of the sources: the infos (cached in files), their covers, the page lists
// of the chapters and the images of the pages. The sources fetch the data (MangaSourceBase), the repository
// merges it into what the app has. The sources are looked up by name (MangaInfo::sourceName) for every call.
//
// There is one info object per manga, every part of the app gets the same one: two copies would both apply an
// update of the chapters to the downloaded pages and to the reading progress.
class MangaRepository : public QObject
{
    Q_OBJECT

public:
    MangaRepository(NetworkManager *networkManager, SourceRegistry *sources, QObject *parent = nullptr);

    // The cached info of a manga, without downloading anything. If there is none it only has its title and url.
    QSharedPointer<MangaInfo> cachedInfo(const QString &sourceName, const QString &mangaUrl,
                                         const QString &title);
    // The cached info if it has chapters (updated in the background if update is set), else downloaded.
    Task<Result<QSharedPointer<MangaInfo>>> loadInfo(QString sourceName, QString mangaUrl, QString title,
                                                     bool update = true);
    // Downloads the info again: merges the chapters into it, saves it if it changed and downloads the cover.
    // While an update of the info runs, that one is returned. Failures are also signalled (infoUpdateFailed).
    Task<Result<>> updateInfo(QSharedPointer<MangaInfo> info, bool updateCover = true);

    // Loads the page list of the chapter, unless it is loaded.
    Task<Result<>> loadPageList(QSharedPointer<MangaInfo> info, int chapter);
    // The image of a page (loads the page list first if needed): the one it was resolved to in this session
    // while it works, else resolved by the source.
    Task<Result<ImageLink>> pageImage(QSharedPointer<MangaInfo> info, int chapter, int page);

    // The download of an image to path (a page of a manga, see MangaInfo::pagePath()), with the headers and the
    // retries of the source.
    FileDownloadDescriptor imageDownload(const QString &sourceName, const ImageLink &image,
                                         const QString &path) const;
    QSharedPointer<DownloadFileJob> downloadImage(const QString &sourceName, const ImageLink &image,
                                                  const QString &path);

signals:
    // An update of an info failed (not for cancelled ones), e.g. one in the background.
    void infoUpdateFailed(QSharedPointer<MangaInfo> info, const Error &error);

private:
    NetworkManager *networkManager;
    SourceRegistry *sources;

    // the infos in use, by source and url (like their files)
    QHash<QString, QWeakPointer<MangaInfo>> liveInfos;
    std::map<const MangaInfo *, Task<Result<>>> runningUpdates;

    // An error if there is no source with the name (anymore). It stays alive while the caller keeps it.
    Result<QSharedPointer<MangaSourceBase>> source(const QString &name) const;
    HttpHeaders requestHeaders(const QString &sourceName, const QString &url) const;

    Task<Result<>> runUpdate(QSharedPointer<MangaInfo> info, bool updateCover);
    // Takes over the details and merges the chapters (removes the downloaded pages of the chapters that are
    // gone).
    Result<> merge(QSharedPointer<MangaInfo> info, const MangaDetails &details);
    Task<> downloadCover(QSharedPointer<MangaInfo> info, QStringList fallbackUrls, bool updateCover);

    void removeChapterPages(QSharedPointer<MangaInfo> info, const QString &chapterUrl);
    void removePagesOfRemovedChapters(QSharedPointer<MangaInfo> info);
    // An image that couldn't be downloaded is resolved again the next time.
    void forgetImage(const QString &url);
};
