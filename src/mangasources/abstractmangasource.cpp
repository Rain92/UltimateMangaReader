#include "abstractmangasource.h"

#include <QSaveFile>

#include "cachefile.h"
#include "imageprocessingnative.h"
#include "mangainfo.h"
#include "perftimer.h"

AbstractMangaSource::AbstractMangaSource(NetworkManager *networkManager)
    : mangaInfoTimeout(2000), networkManager(networkManager), htmlConverter()
{
}

bool AbstractMangaSource::serializeMangaList()
{
    // the old list stays until the new one is complete (the lists have megabytes)
    QSaveFile file(CONF.mangaListDir + name + "_mangalist.dat");
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QDataStream out(&file);
    writeCacheFileHeader(out, CacheFile::MangaList);
    out << mangaList;
    return out.status() == QDataStream::Ok && file.commit();
}

void AbstractMangaSource::loadMangaList()
{
    if (mangaListLoaded)
        return;

    PerfTimer timer("loadMangaList");
    if (deserializeMangaList() && !mangaList.isSortedBy(listOrder))
    {
        mangaList.sortBy(listOrder);
        serializeMangaList();
    }
}

bool AbstractMangaSource::deserializeMangaList()
{
    mangaListLoaded = true;

    QFile file(CONF.mangaListDir + name + "_mangalist.dat");
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDataStream in(&file);
    if (readCacheFileHeader(in, CacheFile::MangaList))
        in >> mangaList;

    bool consistent = mangaList.urls.size() == mangaList.titles.size() &&
                      mangaList.popularityRanks.size() == mangaList.titles.size() &&
                      mangaList.size == mangaList.titles.size();
    if (in.status() != QDataStream::Ok || !consistent)
    {
        qDebug() << "Couldn't read the manga list of" << name;
        mangaList = MangaList();
        return false;
    }

    return true;
}

FileDownloadDescriptor AbstractMangaSource::imageDownload(const ImageLink &image, const QString &path) const
{
    ImageLink request = image;
    request.headers = requestHeaders(image.url) + image.headers;
    return FileDownloadDescriptor(request, path);
}

QSharedPointer<DownloadFileJob> AbstractMangaSource::downloadImage(const ImageLink &image, const QString &path)
{
    auto download = imageDownload(image, path);
    return networkManager->downloadAsScaledImage(download.image, download.path);
}

HttpHeaders AbstractMangaSource::requestHeaders(const QString &url) const
{
    return headerRules.forUrl(url);
}

QSharedPointer<DownloadBufferJob> AbstractMangaSource::download(DownloadRequest request)
{
    request.headers = requestHeaders(request.url) + request.headers;
    return networkManager->download(request);
}

QSharedPointer<DownloadBufferJob> AbstractMangaSource::download(const QString &url, int timeout, int maxRetries)
{
    DownloadRequest request;
    request.url = url;
    request.timeout = timeout;
    request.maxRetries = maxRetries;
    return download(request);
}


QSharedPointer<DownloadFileJob> AbstractMangaSource::downloadAsFile(const QString &url, const QString &localPath)
{
    return networkManager->downloadAsFile(url, localPath, requestHeaders(url));
}

QList<QSize> AbstractMangaSource::probeImageSizes(const QStringList &urls, int timeout, QList<int> *timedOut)
{
    return networkManager->probeImageSizes(urls, timeout, timedOut, requestHeadersFunction());
}

std::function<HttpHeaders(const QString &)> AbstractMangaSource::requestHeadersFunction() const
{
    return [this](const QString &url) { return requestHeaders(url); };
}

Result<ImageLink, QString> AbstractMangaSource::getImageLink(const ImageLink &page)
{
    // by default the page list has the images
    return Ok(page);
}

QString AbstractMangaSource::mangaInfoRequestUrl(const QString &mangaUrl) const
{
    return mangaUrl;
}

Result<QSharedPointer<MangaInfo>, QString> AbstractMangaSource::loadMangaInfo(const QString &mangaUrl,
                                                                              const QString &mangaTitle,
                                                                              bool update)
{
    // the object other parts of the app may hold already (e.g. the favorites), it is filled in place
    auto info = loadCachedMangaInfo(mangaUrl, mangaTitle);

    // an info without a cover is fine, some sources have none for some mangas
    if (!info->chapters.isEmpty())
    {
        if (update)
            updateMangaInfoAsync(info);

        return Ok(info);
    }

    // not cached (or nothing to read when it was saved)
    auto infoR = getMangaInfo(mangaUrl, mangaTitle);
    if (infoR.isErr())
        return infoR;

    // the download ran an event loop: the info was possibly filled meanwhile by another request
    if (info->chapters.isEmpty())
    {
        info->takeOverDetails(*infoR.unwrap());
        info->serialize();
        info->updateCompleted({});
        downloadCoverAsync(info);
    }

    return Ok(info);
}

QSharedPointer<MangaInfo> AbstractMangaSource::liveInfo(const QString &mangaUrl)
{
    auto it = liveInfos.find(mangaUrl);
    if (it == liveInfos.end())
        return {};

    auto info = it.value().toStrongRef();
    if (!info)
        liveInfos.erase(it);
    return info;
}

void AbstractMangaSource::registerInfo(const QSharedPointer<MangaInfo> &info)
{
    liveInfos.insert(info->url, info.toWeakRef());
}

QSharedPointer<MangaInfo> AbstractMangaSource::loadCachedMangaInfo(const QString &mangaUrl,
                                                                  const QString &mangaTitle)
{
    QString path(CONF.mangainfodir(name, mangaUrl) + "mangainfo.dat");

    auto info = liveInfo(mangaUrl);
    if (info)
        return info;

    if (QFile::exists(path))
        info = MangaInfo::deserialize(this, path);

    // missing, unreadable or of another manga
    if (!info || info->url != mangaUrl || info->hostname != name)
    {
        info.reset(new MangaInfo(this));
        info->hostname = name;
        info->title = mangaTitle;
        info->url = mangaUrl;
    }

    registerInfo(info);
    return info;
}

Result<QSharedPointer<MangaInfo>, QString> AbstractMangaSource::getMangaInfo(const QString &mangaUrl,
                                                                             const QString &mangaTitle)
{
    auto job = download(mangaInfoRequestUrl(mangaUrl), mangaInfoTimeout);

    auto info = QSharedPointer<MangaInfo>(new MangaInfo(this));

    info->mangaSource = this;
    info->hostname = name;
    info->url = mangaUrl;
    info->title = mangaTitle;

    if (!job->await())
        return Err(job->errorString);

    auto res = updateMangaInfoFinishedLoading(job, info);
    if (res.isErr())
        return Err(res.unwrapErr());

    auto merged = mergeChapters(info, res.unwrap());
    if (merged.isErr())
        return Err(merged.unwrapErr());

    downloadCoverAsync(info);

    return Ok(info);
}

Result<void, QString> AbstractMangaSource::mergeChapters(QSharedPointer<MangaInfo> info,
                                                         const MangaChapterCollection &newChapters)
{
    // An empty or much shorter list is more likely a broken response (a changed page, a failed request) than
    // the chapters being gone: merging it would delete the downloaded pages of the missing chapters.
    if (!info->chapters.isEmpty() && newChapters.count() < (info->chapters.count() + 1) / 2)
    {
        qDebug() << "Keeping the chapters of" << info->title << ": the source has only" << newChapters.count()
                 << "of" << info->chapters.count();
        return Err(QString("Couldn't load the chapter list of %1.").arg(info->title));
    }

    auto merge = info->chapters.mergeChapters(newChapters);

    if (merge.chaptersRemoved)
        removePagesOfRemovedChapters(info);

    info->updateCompleted(merge);

    return Ok();
}

void AbstractMangaSource::updateMangaInfoAsync(QSharedPointer<MangaInfo> info, bool updateCover)
{
    auto job = download(mangaInfoRequestUrl(info->url), mangaInfoTimeout);

    auto lambda = [info, job, updateCover, this]
    {
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

        auto res = updateMangaInfoFinishedLoading(job, info);
        if (res.isErr())
            return;

        if (mergeChapters(info, res.unwrap()).isErr())
            return;

        downloadCoverAsync(info, updateCover);
        // also when there's no file (e.g. the infos were cleared)
        if (contents() != oldContents || !QFile::exists(info->directory() + "mangainfo.dat"))
            info->serialize();
    };

    executeOnJobCompletion(job, lambda);
}

Result<void, QString> AbstractMangaSource::updatePageList(QSharedPointer<MangaInfo> info, int chapter)
{
    if (chapter >= info->chapters.count() || chapter < 0)
        return Err(QString("Chapter number out of bounds."));

    // expired image links are resolved again by getImageLink()
    if (info->chapters[chapter].pagesLoaded && !info->chapters[chapter].pages.isEmpty())
        return Ok();

    PerfTimer perf("updatePageList");
    const QString chapterUrl = info->chapters[chapter].chapterUrl;
    auto newpagelistR = getPageList(chapterUrl);

    if (!newpagelistR.isOk())
        return Err(newpagelistR.unwrapErr());

    auto newpagelist = newpagelistR.unwrap();
    if (newpagelist.isEmpty())
    {
        qDebug() << "Page list empty:" << chapterUrl;
        return Err(QString("Can't download chapter: pagelist empty."));
    }

    // the download runs an event loop, an update of the chapters can move them meanwhile
    chapter = info->chapters.findChapter(chapterUrl);
    if (chapter < 0)
        return Err(QString("The chapter isn't there anymore."));
    auto &ch = info->chapters[chapter];

    // pages that were downloaded before (e.g. the info was loaded again) might be numbered differently
    removeChapterPages(info, chapterUrl);

    ch.pages.clear();
    for (const auto &link : std::as_const(newpagelist))
        ch.pages.append({link, {}});
    ch.pagesLoaded = true;

    return Ok();
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

void AbstractMangaSource::removeChapterPages(QSharedPointer<MangaInfo> info, const QString &chapterUrl)
{
    // most chapters have none, listing the directory takes a while when it has many pages
    if (!QFile::exists(info->pagePath(chapterUrl, 0)))
        return;

    auto prefix = chapterFilePrefix(chapterUrl);
    removePageFiles(info, [&prefix](const QString &chapter) { return chapter == prefix; });
}

void AbstractMangaSource::removePagesOfRemovedChapters(QSharedPointer<MangaInfo> info)
{
    QSet<QString> prefixes;
    for (const auto &chapter : std::as_const(info->chapters))
        prefixes.insert(chapterFilePrefix(chapter.chapterUrl));

    removePageFiles(info, [&prefixes](const QString &chapter) { return !prefixes.contains(chapter); });
}

QString AbstractMangaSource::defaultCoverPath(QSharedPointer<MangaInfo> mangainfo)
{
    QString filetype = QFileInfo(QUrl(mangainfo->coverUrl).path()).suffix().toLower();
    if (!QStringList({"jpg", "jpeg", "png", "webp", "gif"}).contains(filetype))
        filetype = "jpg";

    return mangainfo->directory() + "cover." + filetype;
}

void AbstractMangaSource::downloadCoverAsync(QSharedPointer<MangaInfo> mangainfo, bool updateCover)
{
    if (mangainfo->coverUrl == "")
        return;

    if (mangainfo->coverPath == "")
        mangainfo->coverPath = defaultCoverPath(mangainfo);

    if (!updateCover && QFile::exists(mangainfo->coverPath))
        return;

    // covers rarely change, and every opening of a manga updates its info
    const qint64 coverMaxAgeDays = 7;
    QFileInfo coverFile(mangainfo->coverPath);
    if (coverFile.exists() && coverFile.lastModified().daysTo(QDateTime::currentDateTime()) < coverMaxAgeDays)
        return;

    auto coverLoadedLambda = [mangainfo]()
    {
        // the favorites make a new thumbnail when they show it (on a worker thread)
        QFile::remove(mangainfo->coverThumbnailPath());
        mangainfo->sendCoverLoaded();
    };
    auto coverjob = downloadAsFile(mangainfo->coverUrl, mangainfo->coverPath);
    executeOnJobCompletion(coverjob, coverLoadedLambda);

    QString fallbackUrl = coverFallbackUrl(mangainfo->coverUrl);
    if (fallbackUrl.isEmpty() || fallbackUrl == mangainfo->coverUrl)
        return;

    auto retry = [this, mangainfo, fallbackUrl, coverLoadedLambda]()
    {
        qDebug() << "Cover not loaded, trying" << fallbackUrl;
        auto job = downloadAsFile(fallbackUrl, mangainfo->coverPath);
        executeOnJobCompletion(job, coverLoadedLambda);
    };

    if (coverjob->hasFailed())
        retry();
    else if (!coverjob->isCompleted)
        QObject::connect(coverjob.get(), &DownloadJobBase::downloadError, coverjob.get(), retry);
}

QString AbstractMangaSource::htmlToPlainText(const QString &str)
{
    htmlConverter.setHtml(str);
    return htmlConverter.toPlainText();
}

void AbstractMangaSource::fillMangaInfo(QSharedPointer<MangaInfo> info, const QString &buffer,
                                        const QRegularExpression &authorrx,
                                        const QRegularExpression &artistrx,
                                        const QRegularExpression &statusrx, const QRegularExpression &yearrx,
                                        const QRegularExpression &genresrx,
                                        const QRegularExpression &summaryrx,
                                        const QRegularExpression &coverrx)
{
    // an empty pattern (a field the source doesn't have) would match anywhere and clear the field
    auto match = [&buffer](const QRegularExpression &rx)
    { return rx.pattern().isEmpty() ? QRegularExpressionMatch() : rx.match(buffer); };

    auto authorrxmatch = match(authorrx);
    auto artistrxmatch = match(artistrx);
    auto statusrxmatch = match(statusrx);
    auto yearrxmatch = match(yearrx);
    auto genresrxmatch = match(genresrx);
    auto summaryrxmatch = match(summaryrx);
    auto coverrxmatch = match(coverrx);

    static const QRegularExpression bbrx(R"(\[.*?\])");

    if (authorrxmatch.hasMatch())
        info->author = htmlToPlainText(authorrxmatch.captured(1)).remove('\n');
    if (artistrxmatch.hasMatch())
        info->artist = htmlToPlainText(artistrxmatch.captured(1)).remove('\n');
    if (statusrxmatch.hasMatch())
        info->status = htmlToPlainText(statusrxmatch.captured(1));
    if (yearrxmatch.hasMatch())
        info->releaseYear = htmlToPlainText(yearrxmatch.captured(1));
    if (genresrxmatch.hasMatch())
        info->genres = htmlToPlainText(genresrxmatch.captured(1))
                           .trimmed()
                           .remove('\n')
                           .replace(", ", " ")
                           .replace("/ ", " ")
                           .replace(",", " ");
    if (summaryrxmatch.hasMatch())
        info->summary = htmlToPlainText(summaryrxmatch.captured(1)).remove(bbrx);
    if (coverrxmatch.hasMatch())
        info->coverUrl = coverrxmatch.captured(1);
}
