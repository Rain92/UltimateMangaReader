#include "mangarepository.h"

#include "perftimer.h"

MangaRepository::MangaRepository(NetworkManager *networkManager, QObject *parent)
    : QObject(parent), networkManager(networkManager), liveInfos(), runningUpdates()
{
    connect(networkManager, &NetworkManager::imageDownloadFailed, this, &MangaRepository::forgetImage);
}

static QString infoKey(const QString &source, const QString &mangaUrl)
{
    return source + '\n' + mangaUrl;
}

QSharedPointer<MangaInfo> MangaRepository::cachedInfo(MangaSourceBase *source, const QString &mangaUrl,
                                                      const QString &title)
{
    auto key = infoKey(source->name, mangaUrl);
    if (auto info = liveInfos.value(key).toStrongRef())
        return info;

    QSharedPointer<MangaInfo> info;
    QString path(CONF.mangainfodir(source->name, mangaUrl) + "mangainfo.dat");
    if (QFile::exists(path))
        info = MangaInfo::deserialize(source, path);

    // missing, unreadable or of another manga
    if (!info || info->url != mangaUrl || info->hostname != source->name)
    {
        info.reset(new MangaInfo(source));
        info->hostname = source->name;
        info->title = title;
        info->url = mangaUrl;
    }

    // the ones that are gone
    if (liveInfos.size() > 64)
        for (auto it = liveInfos.begin(); it != liveInfos.end();)
            it = it.value().isNull() ? liveInfos.erase(it) : std::next(it);
    liveInfos.insert(key, info.toWeakRef());

    return info;
}

static void setDetails(MangaInfo &info, const MangaDetails &details)
{
    info.author = details.author;
    info.artist = details.artist;
    info.releaseYear = details.releaseYear;
    info.genres = details.genres;
    info.status = details.status;
    info.summary = details.summary;
    // a cover that wasn't found this time stays
    if (!details.coverUrl.isEmpty())
        info.coverUrl = details.coverUrl;
}

Task<Result<QSharedPointer<MangaInfo>>> MangaRepository::loadInfo(MangaSourceBase *source, QString mangaUrl,
                                                                  QString title, bool update)
{
    // the object other parts of the app may hold already (e.g. the favorites), it is filled in place
    auto info = cachedInfo(source, mangaUrl, title);

    // an info without a cover is fine, some sources have none for some mangas
    if (!info->chapters.isEmpty())
    {
        if (update)
            updateInfo(info);

        co_return info;
    }

    // not cached (or nothing to read when it was saved)
    auto details = co_await source->fetchMangaInfo(mangaUrl, {});
    if (!details)
        co_return Err(details.error());

    // another request can have filled it meanwhile
    if (info->chapters.isEmpty())
    {
        setDetails(*info, *details);
        info->chapters = details->chapters;
        info->serialize();
        info->updateCompleted({});
        downloadCover(info, details->coverFallbackUrls, true);
    }

    co_return info;
}

Task<Result<>> MangaRepository::updateInfo(QSharedPointer<MangaInfo> info, bool updateCover)
{
    auto running = runningUpdates.find(info.get());
    if (running != runningUpdates.end())
        return running->second;

    auto task = runUpdate(info, updateCover);
    if (!task.isFinished())
    {
        runningUpdates.emplace(info.get(), task);
        task.then([this, key = info.get()](const Result<> &) { runningUpdates.erase(key); });
    }
    return task;
}

Task<Result<>> MangaRepository::runUpdate(QSharedPointer<MangaInfo> info, bool updateCover)
{
    auto details = co_await info->mangaSource->fetchMangaInfo(info->url, info->chapters);

    // the infos of the favorites are updated whenever they are shown, only write the ones that changed
    auto contents = [&info]()
    {
        QStringList list{info->author, info->artist, info->releaseYear, info->genres,
                         info->status, info->summary, info->coverUrl};
        for (const auto &ch : std::as_const(info->chapters))
            list << ch.chapterTitle << ch.chapterUrl;
        return list;
    };
    auto oldContents = contents();

    auto merged = details ? merge(info, *details) : Err(details.error());
    if (!merged)
    {
        if (!merged.error().isCancelled())
        {
            qDebug() << "Couldn't update" << info->title << merged.error().message;
            emit infoUpdateFailed(info, merged.error());
        }
        co_return merged;
    }

    downloadCover(info, details->coverFallbackUrls, updateCover);
    // also when there's no file (e.g. the infos were cleared)
    if (contents() != oldContents || !QFile::exists(info->directory() + "mangainfo.dat"))
        info->serialize();

    co_return {};
}

Result<> MangaRepository::merge(QSharedPointer<MangaInfo> info, const MangaDetails &details)
{
    // An empty or much shorter list is more likely a broken response (a changed page, a failed request) than
    // the chapters being gone: merging it would delete the downloaded pages of the missing chapters.
    if (!info->chapters.isEmpty() && details.chapters.count() < (info->chapters.count() + 1) / 2)
    {
        qDebug() << "Keeping the chapters of" << info->title << ": the source has only" << details.chapters.count()
                 << "of" << info->chapters.count();
        return Err(Error::Parse, QString("Couldn't load the chapter list of %1.").arg(info->title));
    }

    setDetails(*info, details);
    auto merge = info->chapters.mergeChapters(details.chapters);

    if (merge.chaptersRemoved)
        removePagesOfRemovedChapters(info);

    info->updateCompleted(merge);

    return {};
}

Task<Result<>> MangaRepository::loadPageList(QSharedPointer<MangaInfo> info, int chapter)
{
    if (chapter >= info->chapters.count() || chapter < 0)
        co_return Err(QString("Chapter number out of bounds."));

    // the images of the pages are resolved by pageImage()
    if (info->chapters[chapter].pagesLoaded && !info->chapters[chapter].pages.isEmpty())
        co_return {};

    PerfTimer perf("updatePageList");
    const QString chapterUrl = info->chapters[chapter].chapterUrl;
    auto pages = co_await info->mangaSource->fetchPageList(chapterUrl);

    if (!pages)
        co_return Err(pages.error());

    if (pages->isEmpty())
    {
        qDebug() << "Page list empty:" << chapterUrl;
        co_return Err(Error::NotFound, QString("Can't download chapter: pagelist empty."));
    }

    // an update of the chapters can have moved them meanwhile
    chapter = info->chapters.findChapter(chapterUrl);
    if (chapter < 0)
        co_return Err(Error::NotFound, QString("The chapter isn't there anymore."));
    auto &ch = info->chapters[chapter];

    // pages that were downloaded before (e.g. the info was loaded again) might be numbered differently
    removeChapterPages(info, chapterUrl);

    ch.pages.clear();
    for (const auto &link : std::as_const(*pages))
        ch.pages.append({link, {}});
    ch.pagesLoaded = true;

    co_return {};
}

Task<Result<ImageLink>> MangaRepository::pageImage(QSharedPointer<MangaInfo> info, int chapter, int page)
{
    if (chapter < 0 || chapter >= info->chapters.count())
        co_return Err(QString("Chapter number out of bounds."));

    const QString chapterUrl = info->chapters[chapter].chapterUrl;
    if (!info->chapters[chapter].pagesLoaded)
    {
        auto loaded = co_await loadPageList(info, chapter);
        if (!loaded)
            co_return Err(loaded.error());
        info->serialize();
    }

    // page turns, chapter updates and another manga can come in between the downloads: nothing is taken by
    // reference across them, the page is looked up again
    auto findPage = [&info, &chapterUrl, page]() -> MangaPage *
    {
        int c = info->chapters.findChapter(chapterUrl);
        if (c < 0 || page < 0 || page >= info->chapters[c].pages.count())
            return nullptr;
        return &info->chapters[c].pages[page];
    };

    if (!findPage())
        co_return Err(QString("Page index out of bounds."));

    ImageLink image = findPage()->image;
    if (image.expires > 0 && QDateTime::currentSecsSinceEpoch() >= image.expires)
        image = {};

    if (image.isEmpty())
    {
        const ImageLink link = findPage()->link;
        auto resolved = co_await info->mangaSource->resolveImage(link);
        if (!resolved)
            co_return Err(resolved.error());
        image = *resolved;

        if (auto p = findPage(); p && p->link.url == link.url)
            p->image = image;
    }

    co_return image;
}

FileDownloadDescriptor MangaRepository::imageDownload(MangaSourceBase *source, const ImageLink &image,
                                                      const QString &path) const
{
    ImageLink request = image;
    request.headers = source->requestHeaders(image.url) + image.headers;
    return FileDownloadDescriptor(request, path);
}

QSharedPointer<DownloadFileJob> MangaRepository::downloadImage(MangaSourceBase *source, const ImageLink &image,
                                                               const QString &path)
{
    auto download = imageDownload(source, image, path);
    return networkManager->downloadAsScaledImage(download.image, download.path);
}

void MangaRepository::forgetImage(const QString &url)
{
    for (const auto &weakInfo : std::as_const(liveInfos))
    {
        auto info = weakInfo.toStrongRef();
        if (!info)
            continue;

        for (auto &chapter : info->chapters)
            for (auto &page : chapter.pages)
                if (page.image.url == url)
                    page.image = {};
    }
}

static QString defaultCoverPath(const MangaInfo &info)
{
    QString filetype = QFileInfo(QUrl(info.coverUrl).path()).suffix().toLower();
    if (!QStringList({"jpg", "jpeg", "png", "webp", "gif"}).contains(filetype))
        filetype = "jpg";

    return info.directory() + "cover." + filetype;
}

Task<> MangaRepository::downloadCover(QSharedPointer<MangaInfo> info, QStringList fallbackUrls, bool updateCover)
{
    if (info->coverUrl.isEmpty())
        co_return;

    if (info->coverPath.isEmpty())
        info->coverPath = defaultCoverPath(*info);

    if (!updateCover && QFile::exists(info->coverPath))
        co_return;

    // covers rarely change, and every opening of a manga updates its info
    const qint64 coverMaxAgeDays = 7;
    QFileInfo coverFile(info->coverPath);
    if (coverFile.exists() && coverFile.lastModified().daysTo(QDateTime::currentDateTime()) < coverMaxAgeDays)
        co_return;

    QStringList urls{info->coverUrl};
    for (const auto &url : std::as_const(fallbackUrls))
        if (!url.isEmpty() && !urls.contains(url))
            urls.append(url);

    for (int i = 0; i < urls.size(); i++)
    {
        if (i > 0)
            qDebug() << "Cover not loaded, trying" << urls[i];

        auto request = DownloadFileJob::defaultRequest(urls[i], info->mangaSource->requestHeaders(urls[i]));
        // the cdns of some sources answer with 429 for a few seconds while they fetch an image (Comick)
        request.maxRetries = 4;
        request.maxRetryDelay = 2000;
        if (co_await networkManager->downloadAsFile(request, info->coverPath))
        {
            // the favorites make a new thumbnail when they show it (on a worker thread)
            QFile::remove(info->coverThumbnailPath());
            info->sendCoverLoaded();
            co_return;
        }
    }
}

// The page files of a chapter start with the hash of its url.
static QString chapterFilePrefix(const QString &chapterUrl)
{
    return CONF.pagefilename(chapterUrl, 0).section('_', 0, 0);
}

static void removePageFiles(QSharedPointer<MangaInfo> info, const std::function<bool(const QString &)> &ofChapter)
{
    QDir dir(CONF.mangaimagesdir(info->hostname, info->url));
    for (const auto &file : dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files))
        if (ofChapter(file.fileName().section('_', 0, 0)))
            QFile::remove(file.absoluteFilePath());
}

void MangaRepository::removeChapterPages(QSharedPointer<MangaInfo> info, const QString &chapterUrl)
{
    // most chapters have none, listing the directory takes a while when it has many pages
    if (!QFile::exists(info->pagePath(chapterUrl, 0)))
        return;

    auto prefix = chapterFilePrefix(chapterUrl);
    removePageFiles(info, [&prefix](const QString &chapter) { return chapter == prefix; });
}

void MangaRepository::removePagesOfRemovedChapters(QSharedPointer<MangaInfo> info)
{
    QSet<QString> prefixes;
    for (const auto &chapter : std::as_const(info->chapters))
        prefixes.insert(chapterFilePrefix(chapter.chapterUrl));

    removePageFiles(info, [&prefixes](const QString &chapter) { return !prefixes.contains(chapter); });
}
