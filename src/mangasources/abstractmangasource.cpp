#include "abstractmangasource.h"

#include <QSaveFile>
#include <QUrlQuery>

#include "imageprocessingnative.h"
#include "mangainfo.h"
#include "perftimer.h"

AbstractMangaSource::AbstractMangaSource(NetworkManager *networkManager)
    : mangaInfoPostDataStr(), mangaInfoTimeout(2000), networkManager(networkManager), htmlConverter()
{
}

bool AbstractMangaSource::serializeMangaList()
{
    // the old list stays until the new one is complete (the lists have megabytes)
    QSaveFile file(CONF.mangaListDir + name + "_mangalist.dat");
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QDataStream out(&file);
    out.setVersion(CONF.dataStreamVersion);
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
    in.setVersion(CONF.dataStreamVersion);
    in >> mangaList;
    file.close();

    return true;
}

// Entries whose titles are the same (or only differ in what makePathLegal() removes, they share the cache
// directory) are made unique: the one whose info is cached under the title (or else the first one) keeps it,
// the others get the id of the work in their url appended. E.g. MangaDex has hundreds of titles that several
// works share.
void AbstractMangaSource::makeTitlesUnique()
{
    QStringList dirs;
    dirs.reserve(mangaList.size);
    QHash<QString, QList<int>> entries;
    for (int i = 0; i < mangaList.size; i++)
    {
        dirs.append(makePathLegal(mangaList.titles[i]));
        entries[dirs[i]].append(i);
    }

    auto toUrl = [this](const QString &url)
    { return QUrl(mangaList.absoluteUrls || url.startsWith("http") ? url : baseUrl + url); };
    // the url of a cached info can have an older domain
    auto urlKey = [](const QUrl &url)
    {
        QString path = url.path();
        while (path.endsWith('/'))
            path.chop(1);
        return url.hasQuery() ? path + '?' + url.query() : path;
    };
    // the id of the work in the url (e.g. MangaPlus: ...?title_id=<id>)
    static const QRegularExpression uuidrx(R"(^[0-9a-f]{8}-[0-9a-f]{4}-)");
    auto urlId = [](const QUrl &url)
    {
        QUrlQuery query(url);
        if (!query.isEmpty())
            return query.queryItems().first().second;

        QString path = url.path();
        while (path.endsWith('/'))
            path.chop(1);
        QString id = path.section('/', -1);
        return uuidrx.match(id).hasMatch() ? id.left(8) : id;
    };
    auto titleWithId = [&](int i) { return mangaList.titles[i] + " (" + urlId(toUrl(mangaList.urls[i])) + ")"; };
    auto cached = [this](const QString &title)
    { return QFile::exists(CONF.mangainfodir(name, title) + "mangainfo.dat"); };

    QSet<QString> usedDirs(dirs.begin(), dirs.end());
    int renamed = 0;
    for (auto it = entries.cbegin(); it != entries.cend(); ++it)
    {
        const auto &indices = it.value();
        if (indices.size() < 2)
            continue;

        QSharedPointer<MangaInfo> info;
        for (int i : indices)
            if (!info)
                info = liveInfo(mangaList.titles[i]);
        QString path = CONF.mangasourcedir(name) + it.key() + "/mangainfo.dat";
        if (!info && QFile::exists(path))
        {
            try
            {
                info = MangaInfo::deserialize(this, path);
            }
            catch (QException &)
            {
                info.clear();
            }
        }

        int owner = -1;
        if (info)
            for (int i : indices)
                if (urlKey(toUrl(mangaList.urls[i])) == urlKey(QUrl(info->url)))
                    owner = i;
        // the order of the entries can change with the next update, the ones that were opened keep their title
        for (int i : indices)
            if (owner < 0 && !cached(titleWithId(i)))
                owner = i;
        if (owner < 0)
            owner = indices.first();

        for (int i : indices)
        {
            if (i == owner)
                continue;

            QString title = titleWithId(i);
            for (int n = 2; usedDirs.contains(makePathLegal(title)); n++)
                title = titleWithId(i) + " " + QString::number(n);

            usedDirs.insert(makePathLegal(title));
            mangaList.titles[i] = title;
            renamed++;
        }
    }

    if (renamed > 0)
        qDebug() << name << ": made" << renamed << "titles unique";
}

QString AbstractMangaSource::getImagePath(const DownloadImageDescriptor &descriptor)
{
    // save all images as jpg
    auto filetype = ".jpg";

    QString path = QString("%1%2_%3%4")
                       .arg(CONF.mangaimagesdir(name, descriptor.title))
                       .arg(descriptor.chapter)
                       .arg(descriptor.page)
                       .arg(filetype);

    return path;
}

QSharedPointer<DownloadFileJob> AbstractMangaSource::downloadImage(const DownloadImageDescriptor &descriptor)
{
    QString path = getImagePath(descriptor);

    return networkManager->downloadAsScaledImage(descriptor.imageUrl, path);
}

Result<QString, QString> AbstractMangaSource::getImageUrl(const QString &pageurl)
{
    // Default implementation:
    // pageurls are actually already imageurls
    return Ok(pageurl);
}

Result<QSharedPointer<MangaInfo>, QString> AbstractMangaSource::loadMangaInfo(const QString &mangaUrl,
                                                                              const QString &mangaTitle,
                                                                              bool update)
{
    // reload broken favs
    auto usable = [](const QSharedPointer<MangaInfo> &info)
    { return info && info->coverUrl != "" && info->chapters.count() > 0; };

    auto info = liveInfo(mangaTitle);
    if (!usable(info))
    {
        QString path(CONF.mangainfodir(name, mangaTitle) + "mangainfo.dat");
        if (QFile::exists(path))
        {
            try
            {
                info = MangaInfo::deserialize(this, path);
            }
            catch (QException &)
            {
                info.clear();
            }
        }
    }

    if (usable(info))
    {
        registerInfo(info);
        if (update)
            info->mangaSource->updateMangaInfoAsync(info);

        return Ok(info);
    }

    auto infoR = getMangaInfo(mangaUrl, mangaTitle);

    if (infoR.isOk())
    {
        infoR.unwrap()->serialize();
        registerInfo(infoR.unwrap());
    }

    return infoR;
}

QSharedPointer<MangaInfo> AbstractMangaSource::liveInfo(const QString &title)
{
    auto it = liveInfos.find(title);
    if (it == liveInfos.end())
        return {};

    auto info = it.value().toStrongRef();
    if (!info)
        liveInfos.erase(it);
    return info;
}

void AbstractMangaSource::registerInfo(const QSharedPointer<MangaInfo> &info)
{
    liveInfos.insert(info->title, info.toWeakRef());
}

QSharedPointer<MangaInfo> AbstractMangaSource::loadCachedMangaInfo(const QString &mangaUrl,
                                                                  const QString &mangaTitle)
{
    QString path(CONF.mangainfodir(name, mangaTitle) + "mangainfo.dat");

    auto info = liveInfo(mangaTitle);
    if (info)
        return info;

    if (QFile::exists(path))
        info = MangaInfo::deserialize(this, path);

    // missing or unreadable
    if (!info || info->title.isEmpty())
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
    auto job = networkManager->downloadAsString(mangaUrl, mangaInfoTimeout, mangaInfoPostDataStr);

    auto info = QSharedPointer<MangaInfo>(new MangaInfo(this));

    info->mangaSource = this;
    info->hostname = name;
    info->url = mangaUrl;
    info->title = mangaTitle;

    if (!job->await(mangaInfoTimeout))
        return Err(job->errorString);

    int oldnumchapters = info->chapters.count();
    auto res = updateMangaInfoFinishedLoading(job, info);
    if (res.isErr())
        return Err(res.unwrapErr());

    auto merged = mergeChapters(info, res.unwrap(), oldnumchapters);
    if (merged.isErr())
        return Err(merged.unwrapErr());
    chaptersMerged(info);

    downloadCoverAsync(info);

    return Ok(info);
}

Result<void, QString> AbstractMangaSource::mergeChapters(QSharedPointer<MangaInfo> info,
                                                         MangaChapterCollection newChapters,
                                                         int oldnumchapters)
{
    // An empty or much shorter list is more likely a broken response (a changed page, a failed request) than
    // the chapters being gone: merging it would delete the downloaded pages of the missing chapters.
    if (!info->chapters.isEmpty() && newChapters.count() < (info->chapters.count() + 1) / 2)
    {
        qDebug() << "Keeping the chapters of" << info->title << ": the source has only" << newChapters.count()
                 << "of" << info->chapters.count();
        return Err(QString("Couldn't load the chapter list of %1.").arg(info->title));
    }

    auto moveMapping = info->chapters.mergeChapters(newChapters);
    if (!moveMapping.empty())
    {
        info->updated = true;
        removeChapterPages(info, moveMapping);
        reorderChapterPages(info, moveMapping);
    }

    // the first load of an info that was only a placeholder (e.g. a favorite that wasn't cached) brings no
    // new chapters
    bool newchapters = oldnumchapters > 0 && info->chapters.count() > oldnumchapters;
    info->updateCompeted(newchapters, moveMapping);

    return Ok();
}

void AbstractMangaSource::updateMangaInfoAsync(QSharedPointer<MangaInfo> info, bool updateCover)
{
    int oldnumchapters = info->chapters.count();

    auto job = networkManager->downloadAsString(info->url, mangaInfoTimeout, mangaInfoPostDataStr);

    auto lambda = [oldnumchapters, info, job, updateCover, this]
    {
        // the infos of the favorites are updated whenever they are shown, only write the ones that changed
        auto contents = [&info]()
        {
            QStringList list{info->author, info->artist, info->releaseYear, info->genres,
                             info->status, info->summary, info->coverUrl};
            for (const auto &ch : qAsConst(info->chapters))
                list << ch.chapterTitle << ch.chapterUrl;
            return list;
        };
        auto oldContents = contents();

        auto res = updateMangaInfoFinishedLoading(job, info);
        if (res.isErr())
            return;

        if (mergeChapters(info, res.unwrap(), oldnumchapters).isErr())
            return;
        chaptersMerged(info);

        downloadCoverAsync(info, updateCover);
        // also when there's no file (e.g. the infos were cleared)
        if (contents() != oldContents || !QFile::exists(CONF.mangainfodir(name, info->title) + "mangainfo.dat"))
            info->serialize();
    };

    executeOnJobCompletion(job, lambda);
}

Result<void, QString> AbstractMangaSource::updatePageList(QSharedPointer<MangaInfo> info, int chapter)
{
    if (chapter >= info->chapters.count() || chapter < 0)
        return Err(QString("Chapter number out of bounds."));

    if (info->chapters[chapter].pagesLoaded && !info->chapters[chapter].imageUrlList.isEmpty() &&
        info->chapters[chapter].imageUrlList.first() != "")
        return Ok();

    PerfTimer perf("updatePageList");
    const QString chapterUrl = info->chapters[chapter].chapterUrl;
    auto newpagelistR = getPageList(chapterUrl);

    if (!newpagelistR.isOk())
        return Err(newpagelistR.unwrapErr());

    auto newpagelist = newpagelistR.unwrap();

    QMutexLocker locker(info->updateMutex.get());

    // the download runs an event loop, an update of the chapters can move them meanwhile
    if (chapter >= info->chapters.count() || info->chapters[chapter].chapterUrl != chapterUrl)
    {
        chapter = -1;
        for (int c = 0; c < info->chapters.count() && chapter < 0; c++)
            if (info->chapters[c].chapterUrl == chapterUrl)
                chapter = c;
        if (chapter < 0)
            return Err(QString("The chapter isn't there anymore."));
    }
    auto &ch = info->chapters[chapter];

    ch.pageUrlList = newpagelist;

    if (ch.pageUrlList.count() == 0)
    {
        qDebug() << "pageUrls empty" << ch.chapterUrl;
        ch.pageUrlList.clear();
        ch.pageUrlList << "";
        return Err(QString("Can't download chapter: pagelist empty."));
    }
    ch.imageUrlList = QStringList();
    for (int i = 0; i < ch.pageUrlList.count(); i++)
        ch.imageUrlList.append("");
    ch.pagesLoaded = true;

    return Ok();
}

void AbstractMangaSource::removeChapterPages(QSharedPointer<MangaInfo> info,
                                             QList<QPair<int, int>> moveMapping)
{
    QSet<int> remidx;
    for (const auto &[i1, i2] : moveMapping)
    {
        if (i2 < 0)
            remidx.insert(i1);
    }

    auto imagespath = CONF.mangaimagesdir(info->hostname, info->title);
    QDir dir(imagespath);

    for (const auto &info : dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files))
    {
        int chidx = info.baseName().split('_').first().toInt();

        if (remidx.contains(chidx))
            QFile::remove(info.absoluteFilePath());
    }
}

void AbstractMangaSource::reorderChapterPages(QSharedPointer<MangaInfo> info,
                                              QList<QPair<int, int>> moveMapping)
{
    DownloadImageDescriptor id1("", info->title, 0, 0);
    DownloadImageDescriptor id2("", info->title, 0, 0);
    for (const auto &[i1, i2] : moveMapping)
    {
        id1.chapter = i1;
        id2.chapter = i2;
        if (i2 < 0)
        {
        }
        else
        {
            for (int pi = 0; pi < info->chapters[i2].pageUrlList.count(); pi++)
            {
                id1.page = pi;
                id2.page = pi;
                auto oldpath = getImagePath(id1);
                auto newpath = getImagePath(id2) + '_';
                QFile::rename(oldpath, newpath);
            }
        }

        qDebug() << "moving chapter pages:" << i1 << "->" << i2;
    }
    for (const auto &[i1, i2] : moveMapping)
    {
        if (i2 < 0)
            continue;
        id2.chapter = i2;
        for (int pi = 0; pi < info->chapters[i2].pageUrlList.count(); pi++)
        {
            id2.page = pi;
            auto newpath = getImagePath(id2);
            auto oldpath = newpath + '_';
            QFile::rename(oldpath, newpath);
        }
    }
}

QString AbstractMangaSource::defaultCoverPath(QSharedPointer<MangaInfo> mangainfo)
{
    QString filetype = QFileInfo(QUrl(mangainfo->coverUrl).path()).suffix().toLower();
    if (!QStringList({"jpg", "jpeg", "png", "webp", "gif"}).contains(filetype))
        filetype = "jpg";

    return CONF.mangainfodir(name, mangainfo->title) + "cover." + filetype;
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
    auto coverjob = networkManager->downloadAsFile(mangainfo->coverUrl, mangainfo->coverPath);
    executeOnJobCompletion(coverjob, coverLoadedLambda);

    QString fallbackUrl = coverFallbackUrl(mangainfo->coverUrl);
    if (fallbackUrl.isEmpty() || fallbackUrl == mangainfo->coverUrl)
        return;

    auto retry = [this, mangainfo, fallbackUrl, coverLoadedLambda]()
    {
        qDebug() << "Cover not loaded, trying" << fallbackUrl;
        auto job = networkManager->downloadAsFile(fallbackUrl, mangainfo->coverPath);
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
    auto authorrxmatch = authorrx.match(buffer);
    auto artistrxmatch = artistrx.match(buffer);
    auto statusrxmatch = statusrx.match(buffer);
    auto yearrxmatch = yearrx.match(buffer);
    auto genresrxmatch = genresrx.match(buffer);
    auto summaryrxmatch = summaryrx.match(buffer);
    auto coverrxmatch = coverrx.match(buffer);

    QRegularExpression bbrx(R"(\[.*?\])");

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
