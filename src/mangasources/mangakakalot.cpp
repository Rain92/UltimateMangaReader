#include "mangakakalot.h"

#include <QElapsedTimer>

#include <algorithm>

#include "json.h"

using namespace rapidjson;

// all mirrors of the site share the same catalogue and slugs
static const char *const mirrorPattern =
    R"lit(^https?://(?:www\.)?(?:mangakakalove\.com|mangakakalot\.gg|manganato\.gg|natomanga\.com|nelomanga\.(?:net|com))/)lit";

// the site builds its slugs from the title, apostrophes are either dropped or turned into a dash
static QString slugify(const QString &title, bool dropApostrophes)
{
    QString slug;
    bool separator = false;

    bool ascii = std::all_of(title.begin(), title.end(), [](const QChar &c) { return c.unicode() < 0x80; });

    for (const auto &c : ascii ? title.toLower() : title.normalized(QString::NormalizationForm_KD).toLower())
    {
        auto u = c.unicode();
        if ((u >= 'a' && u <= 'z') || (u >= '0' && u <= '9'))
        {
            if (separator && !slug.isEmpty())
                slug += '-';
            separator = false;
            slug += c;
        }
        else if (c.category() == QChar::Mark_NonSpacing || u == 0x2018 || u == 0x2019 ||
                 (dropApostrophes && u == '\''))
        {
            continue;
        }
        else
        {
            separator = true;
        }
    }

    return slug;
}

// "solo-leveling" -> "Solo Leveling", a double dash stands for a dash in the title
static QString titleFromSlug(const QString &slug)
{
    QStringList parts;
    for (const auto &part : slug.split("--", Qt::SkipEmptyParts))
    {
        auto words = part.split('-', Qt::SkipEmptyParts);
        for (auto &word : words)
            word[0] = word[0].toUpper();

        if (!words.isEmpty())
            parts.append(words.join(' '));
    }

    return parts.join(" - ");
}

// "https://www.mangakakalove.com/manga/solo-leveling" -> "solo-leveling"
static QString slugFromUrl(const QString &url)
{
    auto parts = QUrl(url).path().split('/', Qt::SkipEmptyParts);

    return parts.size() == 2 && parts[0] == "manga" ? parts[1] : QString();
}

Mangakakalot::Mangakakalot(NetworkManager *networkManager) : AbstractMangaSource(networkManager)
{
    name = "Mangakakalot";
    // manganato.com / mangakakalot.com are gone, mangakakalot.gg, natomanga.com and nelomanga.net
    // put a Cloudflare challenge in front of every page except the start page
    baseUrl = "https://www.mangakakalove.com/";
    sitemapUrl = baseUrl + "sitemap.xml";
    popularUrl = baseUrl + "genre/all?type=topview&state=all&page=%1";
    chaptersApiUrl = baseUrl + "api/manga/%1/chapters?limit=%2&offset=%3";
    mangaInfoTimeout = 6000;

    // the image cdns only answer with the referer of one of the mirrors
    addImageHost("2xstorage.com");
    addImageHost("waitst.com");
}

// The covers are on several hosts of the cdn and some are missing on the one the site links (503 for good,
// like some pages), imgs-2 had all of them so far.
QString Mangakakalot::coverFallbackUrl(const QString &coverUrl)
{
    QUrl url(coverUrl);
    if (!url.host().endsWith(".2xstorage.com") || !url.path().startsWith("/thumb/"))
        return {};

    url.setHost(url.host() == "imgs-2.2xstorage.com" ? "img-r2.2xstorage.com" : "imgs-2.2xstorage.com");
    return url.toString();
}

void Mangakakalot::chaptersMerged(QSharedPointer<MangaInfo> info)
{
    QString joinedMarker = CONF.mangainfodir(name, info->title) + "joinedpages";
    if (!pendingJoinedMarkers.remove(joinedMarker))
        return;

    QDir().mkpath(CONF.mangainfodir(name, info->title));
    QFile marker(joinedMarker);
    if (!marker.open(QIODevice::WriteOnly))
        qDebug() << "Couldn't write" << joinedMarker;
}

void Mangakakalot::addImageHost(const QString &host)
{
    if (host.isEmpty() || imageHosts.contains(host) ||
        std::any_of(imageHosts.begin(), imageHosts.end(), [&](const QString &h) { return host.endsWith(h); }))
        return;

    imageHosts.insert(host);
    networkManager->addSetCustomRequestHeader(host, "Referer", "https://www.mangakakalove.com/");
}

QString Mangakakalot::normalizeUrl(const QString &url) const
{
    QRegularExpression mirrorrx(mirrorPattern);

    auto match = mirrorrx.match(url);
    if (!match.hasMatch())
        return url;

    return baseUrl + url.mid(match.capturedEnd());
}

bool Mangakakalot::isMirrorUrl(const QString &url) const
{
    QRegularExpression mirrorrx(mirrorPattern);

    return mirrorrx.match(url).hasMatch();
}

QSharedPointer<DownloadStringJob> Mangakakalot::downloadChapterList(const QString &slug, int offset)
{
    return networkManager->downloadAsString(chaptersApiUrl.arg(slug).arg(-1).arg(offset), 8000);
}

Result<QString, QString> Mangakakalot::resolveMangaUrl(const QString &mangaUrl, const QString &mangaTitle)
{
    if (isMirrorUrl(mangaUrl))
        return Ok(normalizeUrl(mangaUrl));

    // urls of the old manganato / mangakakalot domains used ids instead of slugs: find the slug
    // by the title. an empty entry means that the site has no manga with this title
    if (!resolvedLegacyUrls.contains(mangaUrl))
    {
        QStringList slugs;
        for (bool dropApostrophes : {false, true})
        {
            auto slug = slugify(mangaTitle, dropApostrophes);
            if (!slug.isEmpty() && !slugs.contains(slug))
                slugs.append(slug);
        }

        QString url;
        for (const auto &slug : std::as_const(slugs))
        {
            auto job = networkManager->downloadAsString(chaptersApiUrl.arg(slug).arg(1).arg(0), 3000);

            if (job->await(3000, false) && job->bufferStr.contains(R"("success":true)"))
            {
                url = baseUrl + "manga/" + slug;
                qDebug() << "Mangakakalot: mapped old url" << mangaUrl << "to" << url;
                break;
            }

            // unknown slugs are answered with 400 "Invalid slug", don't cache other errors
            if (job->httpStatus != 400 && !job->bufferStr.contains(R"("success":false)"))
                return Err(job->errorString != "" ? job->errorString : QString("Couldn't load the manga."));
        }

        resolvedLegacyUrls.insert(mangaUrl, url);
    }

    auto url = resolvedLegacyUrls.value(mangaUrl);
    if (url.isEmpty())
        return Err(QString("Couldn't find \"%1\" on the new Mangakakalot site.").arg(mangaTitle));

    return Ok(url);
}

bool Mangakakalot::updateMangaList(UpdateProgressToken *token)
{
    QRegularExpression sitemaprx(R"lit(<loc>\s*(https?://[^<\s]*/sitemap-comic-\d+\.xml)\s*</loc>)lit");
    QRegularExpression slugrx(R"lit(<loc>\s*https?://[^/<]+/manga/([^/<\s]+)\s*</loc>)lit");
    QRegularExpression popularrx(R"lit(<h3>\s*<a href="[^"]*/manga/([^/"]+)"\s*title="([^"]*)")lit");
    QRegularExpression idrx(R"lit(^(.+)_(\d+)$)lit");

    QElapsedTimer timer;
    timer.start();

    // a retry only loads the sitemaps the failed attempt didn't get
    auto &state = token->resumeState<MangakakalotUpdateState>();

    auto job = networkManager->downloadAsString(sitemapUrl);

    if (!job->await(7000))
    {
        token->sendError(job->errorString);
        return false;
    }

    QStringList sitemapUrls;
    for (auto &match : getAllRxMatches(sitemaprx, job->bufferStr))
        sitemapUrls.append(normalizeUrl(match.captured(1)));

    if (sitemapUrls.isEmpty())
    {
        token->sendError("Couldn't find the manga sitemaps.");
        return false;
    }

    token->sendProgress(5);

    // the paginated manga lists are rate limited to ~20 requests per minute, so the catalogue
    // comes from the sitemaps. they contain no titles: keep the titles of the previous list
    // and of the popular pages and generate the others from the slug
    QHash<QString, QString> knownTitles;
    QHash<QString, QString> apostropheTitles;
    for (int i = 0; i < mangaList.titles.size() && i < mangaList.urls.size(); i++)
    {
        const auto &title = mangaList.titles[i];
        const auto &url = mangaList.urls[i];
        if (url.startsWith("manga/"))
        {
            knownTitles.insert(url.mid(6), title);
        }
        else
        {
            // the lists of the old domains have id urls, the slugs are built from the titles
            knownTitles.insert(slugify(title, false), title);
            if (title.contains('\''))
                apostropheTitles.insert(slugify(title, true), title);
        }
    }

    QStringList orderedSlugs;
    QSet<QString> slugs;
    auto addSlug = [&](const QString &slug)
    {
        if (slug.isEmpty() || slugs.contains(slug))
            return;

        slugs.insert(slug);
        orderedSlugs.append(slug);
    };

    if (!state.popularDone)
        state.popular.clear();
    for (int page = 1; page <= 2 && !state.popularDone && !token->canceled; page++)
    {
        auto popularJob = networkManager->downloadAsString(popularUrl.arg(page));
        if (!popularJob->await(5000, false))
        {
            qDebug() << "Mangakakalot: popular page failed:" << popularJob->errorString;
            break;
        }

        for (auto &match : getAllRxMatches(popularrx, popularJob->bufferStr))
            state.popular.append({match.captured(1), htmlToPlainText(match.captured(2)).trimmed()});
    }
    // a failed page only degrades the popularity order
    state.popularDone = !token->canceled;

    for (const auto &[slug, title] : std::as_const(state.popular))
    {
        if (!title.isEmpty() && !knownTitles.contains(slug))
            knownTitles.insert(slug, title);
        addSlug(slug);
    }

    token->sendProgress(10);

    QStringList missingSitemaps;
    for (const auto &url : std::as_const(sitemapUrls))
        if (!state.sitemapSlugs.contains(url))
            missingSitemaps.append(url);

    auto lambda = [&](QSharedPointer<DownloadStringJob> job)
    {
        if (!sitemapUrls.contains(job->originalUrl))
            return;

        auto &slugs = state.sitemapSlugs[job->originalUrl];
        for (auto &match : getAllRxMatches(slugrx, job->bufferStr))
            slugs.append(match.captured(1));

        token->sendProgress(10 + 85 * state.sitemapSlugs.size() / sitemapUrls.size());
        qDebug() << "sitemap" << job->originalUrl << "matches:" << slugs.size();
    };

    DownloadQueue queue(networkManager, missingSitemaps, 2, lambda, true, 60000);
    queue.setCancellationToken(&token->canceled);
    queue.start();
    if (!missingSitemaps.isEmpty() && !queue.awaitCompletion())
    {
        token->sendError(queue.lastErrorMessage);
        return false;
    }

    for (const auto &url : std::as_const(sitemapUrls))
        for (const auto &slug : state.sitemapSlugs.value(url))
            addSlug(slug);

    // titles are the key of the manga cache and of the favorites, so every entry needs a unique
    // title that stays the same on the next update: known titles are assigned first. the site has
    // duplicate entries of some works ("x-s-y" / "xs-y", "x--y" / "x-y", "slow-start" / "slow-startt")
    // with different chapters, all of them are kept
    QStringList titles;
    QSet<QString> usedTitles;
    for (const auto &slug : std::as_const(orderedSlugs))
    {
        auto title = knownTitles.value(slug);
        if (usedTitles.contains(title))
            title.clear();
        else if (!title.isEmpty())
            usedTitles.insert(title);
        titles.append(title);
    }

    MangaList mangas;
    mangas.absoluteUrls = false;

    for (int i = 0; i < orderedSlugs.size(); i++)
    {
        const auto &slug = orderedSlugs[i];
        auto title = titles[i];

        if (title.isEmpty())
        {
            // an old title with apostrophes belongs to the slug without them if the dashed one is taken
            title = apostropheTitles.value(slug);
            if (title.isEmpty() || usedTitles.contains(title))
            {
                // the site appends an id to the slugs of different works with the same name
                auto idmatch = slug.contains('_') ? idrx.match(slug) : QRegularExpressionMatch();
                title = titleFromSlug(idmatch.hasMatch() ? idmatch.captured(1) : slug);
                if (idmatch.hasMatch() && (slugs.contains(idmatch.captured(1)) || usedTitles.contains(title)))
                    title += " (" + idmatch.captured(2) + ")";

                if (usedTitles.contains(title))
                    title += " (" + slug + ")";
            }
            usedTitles.insert(title);
        }

        mangas.append(title, "manga/" + slug);
    }

    this->mangaList = mangas;

    qDebug() << "mangas:" << mangas.size << "time:" << timer.elapsed();

    token->sendProgress(100);

    return true;
}

Result<QSharedPointer<MangaInfo>, QString> Mangakakalot::getMangaInfo(const QString &mangaUrl,
                                                                      const QString &mangaTitle)
{
    auto urlR = resolveMangaUrl(mangaUrl, mangaTitle);
    if (urlR.isErr())
        return Err(urlR.unwrapErr());

    // the chapter list comes from the api: load it in parallel to the manga page
    auto slug = slugFromUrl(urlR.unwrap());
    if (!slug.isEmpty())
        prefetchChapters(slug, downloadChapterList(slug, 0));

    auto res = AbstractMangaSource::getMangaInfo(urlR.unwrap(), mangaTitle);
    prefetchedChapters.remove(slug);

    return res;
}

void Mangakakalot::prefetchChapters(const QString &slug, QSharedPointer<DownloadStringJob> job)
{
    PrefetchedChapters prefetched{job, {}};
    prefetched.age.start();
    prefetchedChapters.insert(slug, prefetched);
}

QSharedPointer<DownloadStringJob> Mangakakalot::takePrefetchedChapters(const QString &slug)
{
    // left from an update whose manga page couldn't be loaded, the chapters may have changed since
    const qint64 maxAgeMs = 60 * 1000;

    auto prefetched = prefetchedChapters.take(slug);
    if (!prefetched.job || prefetched.age.elapsed() > maxAgeMs)
        return {};
    return prefetched.job;
}

void Mangakakalot::updateMangaInfoAsync(QSharedPointer<MangaInfo> info, bool updateCover)
{
    auto urlR = resolveMangaUrl(info->url, info->title);
    if (urlR.isErr())
    {
        qDebug() << urlR.unwrapErr();
        return;
    }

    info->url = urlR.unwrap();

    // load the chapter list before the manga page, so updateMangaInfoFinishedLoading doesn't
    // have to wait for it on the ui thread
    auto slug = slugFromUrl(info->url);
    if (slug.isEmpty())
    {
        AbstractMangaSource::updateMangaInfoAsync(info, updateCover);
        return;
    }

    auto job = downloadChapterList(slug, 0);
    executeOnJobCompletion(job,
                           [this, info, updateCover, slug, job]()
                           {
                               prefetchChapters(slug, job);
                               AbstractMangaSource::updateMangaInfoAsync(info, updateCover);
                           });
    // without it the update loads the chapter list itself (e.g. after a rate limit)
    QObject::connect(job.get(), &DownloadJobBase::downloadError, job.get(),
                     [this, info, updateCover]() { AbstractMangaSource::updateMangaInfoAsync(info, updateCover); });
}

Result<MangaChapterCollection, QString> Mangakakalot::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression slugrx(R"lit(data-comic-slug="([^"]+)")lit");
    QRegularExpression authorrx(R"lit(<li>Author\(s\)\s*:(.*?)</li>)lit",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression artistrx;
    QRegularExpression statusrx(R"lit(<li>Status\s*:([^<]*)</li>)lit");
    QRegularExpression yearrx;
    QRegularExpression genresrx(R"lit(<li class="genres">Genres\s*:(.*?)</li>)lit",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression summaryrx(R"lit(<div id="contentBox"[^>]*>(?:\s*<h2>.*?</h2>)?(.*?)</div>)lit",
                                 QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression coverrx(R"lit(<div class="manga-info-pic">\s*<img src="([^"]*)")lit");
    QRegularExpression ogcoverrx(R"lit(<meta property="og:image" content="([^"]*)")lit");
    QRegularExpression genrerx(R"lit(<a[^>]*>([^<]*)</a>)lit");
    QRegularExpression entityrx(R"lit(&(?:quot|amp|lt|gt|#0?39);)lit");

    auto slugmatch = slugrx.match(job->bufferStr);
    if (!slugmatch.hasMatch())
        return Err(QString("Couldn't parse the manga page."));

    auto slug = slugmatch.captured(1);

    fillMangaInfo(info, job->bufferStr, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx, coverrx);

    info->author = info->author.trimmed();
    info->status = info->status.trimmed();
    info->summary = info->summary.trimmed();
    if (info->summary.contains(entityrx))
        info->summary = htmlToPlainText(info->summary).trimmed();

    auto genresmatch = genresrx.match(job->bufferStr);
    if (genresmatch.hasMatch())
    {
        QStringList genres;
        for (auto &match : getAllRxMatches(genrerx, genresmatch.captured(1)))
            genres.append(htmlToPlainText(match.captured(1)).trimmed());
        info->genres = genres.join(", ");
    }

    if (info->coverUrl.isEmpty())
    {
        auto ogcovermatch = ogcoverrx.match(job->bufferStr);
        if (ogcovermatch.hasMatch())
            info->coverUrl = ogcovermatch.captured(1);
    }
    // some mangas have only the cdn host as cover, the site shows its placeholder then
    if (QUrl(info->coverUrl).path().length() <= 1)
        info->coverUrl = baseUrl + "images/404-avatar.webp";
    else
        addImageHost(QUrl(info->coverUrl).host());

    // the chapter list is loaded from the api, newest first. the first request is usually
    // started by getMangaInfo or updateMangaInfoAsync already
    QList<MangaChapter> chapters;
    QSet<QString> chapterUrls;
    int offset = 0;
    auto chaptersJob = takePrefetchedChapters(slug);
    for (int request = 0; request < 10; request++)
    {
        if (request > 0 || !chaptersJob || chaptersJob->hasFailed())
            chaptersJob = downloadChapterList(slug, offset);

        if (!chaptersJob->await(8000))
            return Err(chaptersJob->errorString);

        bool hasMore = false;
        try
        {
            Document doc;
            ParseResult res = doc.Parse(chaptersJob->buffer.data());

            if (!res || !doc.IsObject() || !doc.HasMember("success") || !doc["success"].IsBool() ||
                !doc["success"].GetBool() || !doc.HasMember("data") || !doc["data"].IsObject())
                return Err(QString("Couldn't load the chapter list."));

            auto &data = doc["data"];
            if (!data.HasMember("chapters") || !data["chapters"].IsArray())
                return Err(QString("Couldn't load the chapter list."));

            auto chapterArray = data["chapters"].GetArray();
            for (const auto &ch : chapterArray)
            {
                if (!ch.IsObject() || !ch.HasMember("chapter_slug") || !ch["chapter_slug"].IsString())
                    continue;

                auto url =
                    baseUrl + "manga/" + slug + '/' + QString::fromUtf8(ch["chapter_slug"].GetString());
                if (chapterUrls.contains(url))
                    continue;
                chapterUrls.insert(url);

                QString number;
                if (ch.HasMember("chapter_num") && ch["chapter_num"].IsNumber())
                    number = QString::number(ch["chapter_num"].GetDouble());

                QString title;
                if (ch.HasMember("chapter_name") && ch["chapter_name"].IsString())
                    title = QString::fromUtf8(ch["chapter_name"].GetString()).simplified();
                if (title.contains('&'))
                    title = htmlToPlainText(title);
                if (title.isEmpty())
                    title = "Chapter " + number;

                MangaChapter chapter(title, url);
                chapter.chapterNumber = number;
                chapters.append(chapter);
            }

            offset += chapterArray.Size();

            if (data.HasMember("pagination") && data["pagination"].IsObject())
            {
                auto &pagination = data["pagination"];
                if (pagination.HasMember("has_more") && pagination["has_more"].IsBool())
                    hasMore = pagination["has_more"].GetBool() && chapterArray.Size() > 0;
            }
        }
        catch (const JsonError &)
        {
            return Err(QString("Couldn't parse the chapter list."));
        }

        if (!hasMore)
            break;
    }

    // Page lists from before split pages were joined (see joinSplitPages()) are loaded again once, unless
    // pages were downloaded already: their files and the reading progress count the pages of the old list.
    QString joinedMarker = CONF.mangainfodir(name, info->title) + "joinedpages";
    bool reloadUnjoined = !QFile::exists(joinedMarker);

    for (int c = 0; c < info->chapters.count(); c++)
    {
        auto &chapter = info->chapters[c];
        if (!chapter.pagesLoaded)
            continue;

        auto pageOnDisk = [&](int p)
        { return QFile::exists(getImagePath(DownloadImageDescriptor("", info->title, c, p))); };

        // the image urls of chapters from the old domains are dead: only downloaded pages count as
        // loaded, so the chapters that aren't completely downloaded are loaded again from the new site
        if (!isMirrorUrl(chapter.chapterUrl))
        {
            for (int p = 0; p < chapter.imageUrlList.count(); p++)
                if (!pageOnDisk(p))
                    chapter.imageUrlList[p] = "";
            continue;
        }

        if (reloadUnjoined && !chapter.pageUrlList.isEmpty() && !pageOnDisk(0))
        {
            bool anyOnDisk = false;
            for (int p = 1; p < chapter.pageUrlList.count() && !anyOnDisk; p++)
                anyOnDisk = pageOnDisk(p);
            if (!anyOnDisk)
                chapter.pagesLoaded = false;
        }
    }

    // the marker is written in chaptersMerged(): the new list can still be rejected
    if (reloadUnjoined)
        pendingJoinedMarkers.insert(joinedMarker);

    MangaChapterCollection newchapters;
    for (int i = chapters.size() - 1; i >= 0; i--)
        newchapters.append(chapters[i]);

    return Ok(newchapters);
}

Result<QStringList, QString> Mangakakalot::getPageList(const QString &chapterUrl)
{
    QRegularExpression cdnrx(R"lit(cdns\s*=\s*\[\s*"([^"]+)")lit");
    QRegularExpression imagesrx(R"lit(chapterImages\s*=\s*\[([^\]]*)\])lit");
    QRegularExpression pathrx(R"lit("([^"]+)")lit");
    QRegularExpression imgrx(R"lit(<img\s+src=['"]([^'"]+)['"])lit");

    auto url = normalizeUrl(chapterUrl);
    if (!isMirrorUrl(url))
        return Err(QString("Outdated chapter link, please reload the manga."));

    auto job = networkManager->downloadAsString(url);

    if (!job->await(7000))
        return Err(job->errorString);

    QStringList imageUrls;

    auto cdnmatch = cdnrx.match(job->bufferStr);
    auto imagesmatch = imagesrx.match(job->bufferStr);
    if (cdnmatch.hasMatch() && imagesmatch.hasMatch())
    {
        auto cdn = cdnmatch.captured(1).remove('\\');
        while (cdn.endsWith('/'))
            cdn.chop(1);

        for (auto &match : getAllRxMatches(pathrx, imagesmatch.captured(1)))
        {
            auto path = match.captured(1).remove('\\');
            if (path.startsWith("http"))
                imageUrls.append(path);
            else
                imageUrls.append(cdn + '/' + (path.startsWith('/') ? path.mid(1) : path));
        }
    }

    if (imageUrls.isEmpty())
    {
        int spos = job->bufferStr.indexOf(R"(class="container-chapter-reader")");
        if (spos >= 0)
        {
            int epos = job->bufferStr.indexOf("</div>", spos);
            for (auto &match : getAllRxMatches(imgrx, job->bufferStr, spos, epos))
                imageUrls.append(match.captured(1));
        }
    }

    for (const auto &imageUrl : std::as_const(imageUrls))
        addImageHost(QUrl(imageUrl).host());

    return joinSplitPages(imageUrls);
}

// The site cuts images taller than 1500 px into pieces of 1500 px and the rest: a page of 1100x1565 becomes
// 1100x1500 and 1100x65, which would be a page of its own showing a thin line. The parts of a page are
// joined again ("<piece url>|append:<rest url>", the image job stitches them), the pieces of long strips
// (manhwa) stay pages of their own.
Result<QStringList, QString> Mangakakalot::joinSplitPages(const QStringList &imageUrls)
{
    QElapsedTimer timer;
    timer.start();
    // The sizes that didn't come in time are asked for once more: the list is stored with the chapter, pages
    // that weren't joined because of a slow connection would stay that way.
    QList<int> timedOut;
    auto sizes = networkManager->probeImageSizes(imageUrls, 8000, &timedOut);
    if (!timedOut.isEmpty())
    {
        QStringList again;
        for (int i : std::as_const(timedOut))
            again.append(imageUrls[i]);
        QList<int> stillTimedOut;
        auto moreSizes = networkManager->probeImageSizes(again, 8000, &stillTimedOut);
        for (int k = 0; k < timedOut.size(); k++)
            sizes[timedOut[k]] = moreSizes[k];
        if (stillTimedOut.size() * 4 > imageUrls.size())
            return Err(QString("The pages of the chapter couldn't be checked, the connection is too slow."));
    }

    QStringList pages;
    for (const auto &page : pairSplitImages(sizes))
        pages.append(page.second < 0 ? imageUrls[page.first]
                                     : imageUrls[page.first] + "|append:" + imageUrls[page.second]);

    int unknown = std::count_if(sizes.begin(), sizes.end(), [](const QSize &s) { return !s.isValid(); });
    qDebug() << "Page sizes probed in" << timer.elapsed() << "ms:" << imageUrls.size() << "images," << unknown
             << "unknown ->" << pages.size() << "pages";

    return Ok(pages);
}

QList<QPair<int, int>> Mangakakalot::pairSplitImages(const QList<QSize> &sizes)
{
    const int pieceHeight = 1500;
    const int n = sizes.size();

    auto known = [&](int i) { return i >= 0 && i < n && sizes[i].isValid(); };
    auto isPiece = [&](int i) { return known(i) && sizes[i].height() == pieceHeight; };
    // the middle and the end of long strips follow a piece of the same width
    auto continuesStrip = [&](int i) { return isPiece(i - 1) && sizes[i - 1].width() == sizes[i].width(); };
    // the rest of the image of a piece: together they have the proportions of a page, or the rest is short
    // (also at the end of a long strip)
    auto isRestOf = [&](int rest, int width, bool ofStrip = false)
    {
        if (!known(rest) || sizes[rest].width() != width || sizes[rest].height() >= pieceHeight)
            return false;
        int height = sizes[rest].height();
        return height <= pieceHeight / 4 || (!ofStrip && pieceHeight + height <= 2 * width);
    };

    int pieces = 0, pairs = 0, unknown = 0;
    for (int i = 0; i < n; i++)
    {
        if (!known(i))
            unknown++;
        else if (isPiece(i))
        {
            pieces++;
            if (!continuesStrip(i) && isRestOf(i + 1, sizes[i].width()))
                pairs++;
        }
    }

    // Some images are missing at the source (they always fail). Where most pages are a piece and a rest, a
    // missing one is taken as the part its neighbours lack, the page shows the other part only then.
    bool pagesSplit = pairs > 0 && pairs * 2 >= pieces && unknown * 4 <= n;
    // the width of the closest piece, for a missing piece
    auto pieceWidthNear = [&](int i)
    {
        for (int d = 1; d < n; d++)
            for (int j : {i - d, i + d})
                if (isPiece(j))
                    return sizes[j].width();
        return -1;
    };

    QList<QPair<int, int>> pages;
    for (int i = 0; i < n; i++)
    {
        if (isPiece(i))
        {
            int width = sizes[i].width();
            bool strip = continuesStrip(i);
            bool nextIsPieceOfMissingOne = !known(i + 1) && isRestOf(i + 2, pieceWidthNear(i + 1));
            if (isRestOf(i + 1, width, strip) ||
                (pagesSplit && !strip && i + 1 < n && !known(i + 1) && !nextIsPieceOfMissingOne))
            {
                pages.append({i, i + 1});
                i++;
                continue;
            }
        }
        else if (pagesSplit && !known(i) && isRestOf(i + 1, pieceWidthNear(i)))
        {
            pages.append({i, i + 1});
            i++;
            continue;
        }

        pages.append({i, -1});
    }

    return pages;
}
