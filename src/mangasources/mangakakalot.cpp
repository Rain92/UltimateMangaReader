#include "mangakakalot.h"

#include <QElapsedTimer>

#include <algorithm>

#include "json.h"

using namespace rapidjson;

// all mirrors of the site share the same catalogue and slugs
static const char *const mirrorPattern =
    R"lit(^https?://(?:www\.)?(?:mangakakalove\.com|mangakakalot\.gg|manganato\.gg|natomanga\.com|nelomanga\.(?:net|com))/)lit";

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

Mangakakalot::Mangakakalot(NetworkManager *networkManager) : MangaSourceBase(networkManager)
{
    name = "Mangakakalot";
    // manganato.com / mangakakalot.com are gone, mangakakalot.gg, natomanga.com and nelomanga.net
    // put a Cloudflare challenge in front of every page except the start page
    baseUrl = "https://www.mangakakalove.com/";
    sitemapUrl = baseUrl + "sitemap.xml";
    popularUrl = baseUrl + "genre/all?type=topview&state=all&page=%1";
    chaptersApiUrl = baseUrl + "api/manga/%1/chapters?limit=%2&offset=%3";

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

void Mangakakalot::addImageHost(const QString &host)
{
    if (host.isEmpty() || imageHosts.contains(host) ||
        std::any_of(imageHosts.begin(), imageHosts.end(), [&](const QString &h) { return host.endsWith(h); }))
        return;

    imageHosts.insert(host);
    headerRules.add(host, "Referer", "https://www.mangakakalove.com/");
}

QString Mangakakalot::normalizeUrl(const QString &url) const
{
    QRegularExpression mirrorrx(mirrorPattern);

    auto match = mirrorrx.match(url);
    if (!match.hasMatch())
        return url;

    return baseUrl + url.mid(match.capturedEnd());
}

QSharedPointer<DownloadBufferJob> Mangakakalot::downloadChapterList(const QString &slug, int offset)
{
    return download(chaptersApiUrl.arg(slug).arg(-1).arg(offset), 8000);
}

Task<MangaListUpdate> Mangakakalot::fetchMangaList(UpdateProgressToken *token, MangaList current)
{
    QRegularExpression sitemaprx(R"lit(<loc>\s*(https?://[^<\s]*/sitemap-comic-\d+\.xml)\s*</loc>)lit");
    QRegularExpression slugrx(R"lit(<loc>\s*https?://[^/<]+/manga/([^/<\s]+)\s*</loc>)lit");
    QRegularExpression popularrx(R"lit(<h3>\s*<a href="[^"]*/manga/([^/"]+)"\s*title="([^"]*)")lit");
    QRegularExpression idrx(R"lit(^(.+)_(\d+)$)lit");

    QElapsedTimer timer;
    timer.start();

    // a retry only loads the sitemaps the failed attempt didn't get
    auto &state = token->resumeState<MangakakalotUpdateState>();

    auto job = download(sitemapUrl, 7000);

    if (!co_await job)
        co_return MangaListUpdate::failed(job->error);

    QStringList sitemapUrls;
    for (auto &match : getAllRxMatches(sitemaprx, job->text()))
        sitemapUrls.append(normalizeUrl(match.captured(1)));

    if (sitemapUrls.isEmpty())
        co_return MangaListUpdate::failed({Error::Parse, "Couldn't find the manga sitemaps.", 0});

    token->sendProgress(5);

    // the paginated manga lists are rate limited to ~20 requests per minute, so the catalogue
    // comes from the sitemaps. they contain no titles: the ones of the popular pages and of the
    // previous list are taken, the others are made from the slug
    QHash<QString, QString> knownTitles;
    for (int i = 0; i < current.titles.size() && i < current.urls.size(); i++)
        if (current.urls[i].startsWith("manga/"))
            knownTitles.insert(current.urls[i].mid(6), current.titles[i]);

    QStringList orderedSlugs;
    QSet<QString> slugs;
    auto addSlug = [&](const QString &slug)
    {
        if (slug.isEmpty() || slugs.contains(slug))
            return;

        slugs.insert(slug);
        orderedSlugs.append(slug);
    };

    // a failed page only degrades the popularity order
    Error popularError;
    if (!state.popularDone)
        state.popular.clear();
    for (int page = 1; page <= 2 && !state.popularDone && !token->canceled; page++)
    {
        auto popularJob = download(popularUrl.arg(page), 5000, 0);
        if (!co_await popularJob)
        {
            qDebug() << "Mangakakalot: popular page failed:" << popularJob->error.message;
            popularError = popularJob->error;
            popularError.message = "Couldn't load the popular titles: " + popularError.message;
            break;
        }

        for (auto &match : getAllRxMatches(popularrx, popularJob->text()))
            state.popular.append({match.captured(1), htmlToPlainText(match.captured(2)).trimmed()});
    }
    state.popularDone = !token->canceled;

    for (const auto &[slug, title] : std::as_const(state.popular))
    {
        if (!title.isEmpty())
            knownTitles.insert(slug, title);
        addSlug(slug);
    }

    token->sendProgress(10);

    QStringList missingSitemaps;
    for (const auto &url : std::as_const(sitemapUrls))
        if (!state.sitemapSlugs.contains(url))
            missingSitemaps.append(url);

    auto lambda = [&](QSharedPointer<DownloadBufferJob> job)
    {
        if (!sitemapUrls.contains(job->request.url))
            return;

        auto &slugs = state.sitemapSlugs[job->request.url];
        for (auto &match : getAllRxMatches(slugrx, job->text()))
            slugs.append(match.captured(1));

        token->sendProgress(10 + 85 * state.sitemapSlugs.size() / sitemapUrls.size());
        qDebug() << "sitemap" << job->request.url << "matches:" << slugs.size();
    };

    DownloadQueue queue(networkManager, missingSitemaps, 2, lambda, true, 60000, requestHeadersFunction());
    queue.setCancellationToken(&token->canceled);
    queue.start();
    if (!missingSitemaps.isEmpty() && !co_await queue)
        co_return MangaListUpdate::failed(queue.lastError);

    for (const auto &url : std::as_const(sitemapUrls))
        for (const auto &slug : state.sitemapSlugs.value(url))
            addSlug(slug);

    // the site has duplicate entries of some works ("x-s-y" / "xs-y", "x--y" / "x-y", "slow-start" /
    // "slow-startt") with different chapters, all of them are kept
    MangaList mangas;
    mangas.absoluteUrls = false;

    for (const auto &slug : std::as_const(orderedSlugs))
    {
        auto title = knownTitles.value(slug);
        if (title.isEmpty())
        {
            // the site appends an id to the slugs of different works with the same name
            auto idmatch = slug.contains('_') ? idrx.match(slug) : QRegularExpressionMatch();
            title = titleFromSlug(idmatch.hasMatch() ? idmatch.captured(1) : slug);
            if (idmatch.hasMatch() && slugs.contains(idmatch.captured(1)))
                title += " (" + idmatch.captured(2) + ")";
        }

        mangas.append(title, "manga/" + slug);
    }

    qDebug() << "mangas:" << mangas.size << "time:" << timer.elapsed();

    token->sendProgress(100);

    if (!popularError.message.isEmpty())
        co_return MangaListUpdate::partial(mangas, popularError);
    co_return MangaListUpdate::complete(mangas);
}

Task<Result<MangaDetails>> Mangakakalot::fetchMangaInfo(QString mangaUrl, MangaChapterCollection)
{
    QRegularExpression slugrx(R"lit(data-comic-slug="([^"]+)")lit");
    QRegularExpression authorrx(R"lit(<li>Author\(s\)\s*:(.*?)</li>)lit",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression statusrx(R"lit(<li>Status\s*:([^<]*)</li>)lit");
    QRegularExpression genresrx(R"lit(<li class="genres">Genres\s*:(.*?)</li>)lit",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression summaryrx(R"lit(<div id="contentBox"[^>]*>(?:\s*<h2>.*?</h2>)?(.*?)</div>)lit",
                                 QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression coverrx(R"lit(<div class="manga-info-pic">\s*<img src="([^"]*)")lit");
    QRegularExpression ogcoverrx(R"lit(<meta property="og:image" content="([^"]*)")lit");
    QRegularExpression genrerx(R"lit(<a[^>]*>([^<]*)</a>)lit");
    QRegularExpression entityrx(R"lit(&(?:quot|amp|lt|gt|#0?39);)lit");

    static const QRegularExpression bbrx(R"(\[.*?\])");

    // the chapter list comes from the api: its first page loads in parallel to the manga page
    auto urlSlug = slugFromUrl(mangaUrl);
    QSharedPointer<DownloadBufferJob> chaptersJob;
    if (!urlSlug.isEmpty())
        chaptersJob = downloadChapterList(urlSlug, 0);

    auto job = download(mangaUrl, 6000);
    if (!co_await job)
        co_return Err(job->error);

    const auto html = job->text();
    auto slugmatch = slugrx.match(html);
    if (!slugmatch.hasMatch())
        co_return Err(Error::Parse, "Couldn't parse the manga page.");

    auto slug = slugmatch.captured(1);

    MangaDetails details;
    auto authormatch = authorrx.match(html);
    if (authormatch.hasMatch())
        details.author = htmlToPlainText(authormatch.captured(1)).remove('\n').trimmed();
    auto statusmatch = statusrx.match(html);
    if (statusmatch.hasMatch())
        details.status = htmlToPlainText(statusmatch.captured(1)).trimmed();
    auto summarymatch = summaryrx.match(html);
    if (summarymatch.hasMatch())
        details.summary = htmlToPlainText(summarymatch.captured(1)).remove(bbrx).trimmed();
    if (details.summary.contains(entityrx))
        details.summary = htmlToPlainText(details.summary).trimmed();

    auto genresmatch = genresrx.match(html);
    if (genresmatch.hasMatch())
    {
        QStringList genres;
        for (auto &match : getAllRxMatches(genrerx, genresmatch.captured(1)))
            genres.append(htmlToPlainText(match.captured(1)).trimmed());
        details.genres = genres.join(", ");
    }

    details.coverUrl = capture(coverrx, html);
    if (details.coverUrl.isEmpty())
        details.coverUrl = capture(ogcoverrx, html);
    // some mangas have only the cdn host as cover, the site shows its placeholder then
    if (QUrl(details.coverUrl).path().length() <= 1)
        details.coverUrl = baseUrl + "images/404-avatar.webp";
    else
        addImageHost(QUrl(details.coverUrl).host());
    auto fallbackUrl = coverFallbackUrl(details.coverUrl);
    if (!fallbackUrl.isEmpty())
        details.coverFallbackUrls.append(fallbackUrl);

    // the chapter list is loaded from the api, newest first
    QList<MangaChapter> chapters;
    QSet<QString> chapterUrls;
    int offset = 0;
    if (slug != urlSlug)
        chaptersJob.reset();
    for (int request = 0; request < 10; request++)
    {
        if (request > 0 || !chaptersJob)
            chaptersJob = downloadChapterList(slug, offset);

        if (!co_await chaptersJob)
            co_return Err(chaptersJob->error);

        bool hasMore = false;
        try
        {
            Document doc;
            ParseResult res = doc.Parse(chaptersJob->buffer.data());

            if (!res || !doc.IsObject() || !doc.HasMember("success") || !doc["success"].IsBool() ||
                !doc["success"].GetBool() || !doc.HasMember("data") || !doc["data"].IsObject())
                co_return Err(Error::Parse, "Couldn't load the chapter list.");

            auto &data = doc["data"];
            if (!data.HasMember("chapters") || !data["chapters"].IsArray())
                co_return Err(Error::Parse, "Couldn't load the chapter list.");

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

                QString title;
                if (ch.HasMember("chapter_name") && ch["chapter_name"].IsString())
                    title = QString::fromUtf8(ch["chapter_name"].GetString()).simplified();
                if (title.contains('&'))
                    title = htmlToPlainText(title);
                if (title.isEmpty() && ch.HasMember("chapter_num") && ch["chapter_num"].IsNumber())
                    title = "Chapter " + QString::number(ch["chapter_num"].GetDouble());
                if (title.isEmpty())
                    title = "Chapter";

                chapters.append(MangaChapter(title, url));
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
            co_return Err(Error::Parse, "Couldn't parse the chapter list.");
        }

        if (!hasMore)
            break;
    }

    for (int i = chapters.size() - 1; i >= 0; i--)
        details.chapters.append(chapters[i]);

    co_return details;
}

Task<Result<QList<ImageLink>>> Mangakakalot::fetchPageList(QString chapterUrl)
{
    QRegularExpression cdnrx(R"lit(cdns\s*=\s*\[\s*"([^"]+)")lit");
    QRegularExpression imagesrx(R"lit(chapterImages\s*=\s*\[([^\]]*)\])lit");
    QRegularExpression pathrx(R"lit("([^"]+)")lit");
    QRegularExpression imgrx(R"lit(<img\s+src=['"]([^'"]+)['"])lit");

    auto job = download(chapterUrl, 7000);

    if (!co_await job)
        co_return Err(job->error);
    const auto html = job->text();

    QStringList imageUrls;

    auto cdnmatch = cdnrx.match(html);
    auto imagesmatch = imagesrx.match(html);
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
        int spos = html.indexOf(R"(class="container-chapter-reader")");
        if (spos >= 0)
        {
            int epos = html.indexOf("</div>", spos);
            for (auto &match : getAllRxMatches(imgrx, html, spos, epos))
                imageUrls.append(match.captured(1));
        }
    }

    for (const auto &imageUrl : std::as_const(imageUrls))
        addImageHost(QUrl(imageUrl).host());

    co_return co_await joinSplitPages(imageUrls);
}

// The site cuts images taller than 1500 px into pieces of 1500 px and the rest: a page of 1100x1565 becomes
// 1100x1500 and 1100x65, which would be a page of its own showing a thin line. The parts of a page are
// joined again (the image job stitches the appendUrl below the page), the pieces of long strips (manhwa) stay
// pages of their own.
Task<Result<QList<ImageLink>>> Mangakakalot::joinSplitPages(QStringList imageUrls)
{
    QElapsedTimer timer;
    timer.start();
    // The sizes that didn't come in time are asked for once more: the list is stored with the chapter, pages
    // that weren't joined because of a slow connection would stay that way.
    auto probe = co_await probeImageSizes(imageUrls, 8000);
    auto &sizes = probe.sizes;
    if (!probe.timedOut.isEmpty())
    {
        QStringList again;
        for (int i : std::as_const(probe.timedOut))
            again.append(imageUrls[i]);
        auto more = co_await probeImageSizes(again, 8000);
        for (int k = 0; k < probe.timedOut.size(); k++)
            sizes[probe.timedOut[k]] = more.sizes[k];
        if (more.timedOut.size() * 4 > imageUrls.size())
            co_return Err(Error::Timeout, "The pages of the chapter couldn't be checked, the connection is too slow.");
    }

    QList<ImageLink> pages;
    for (const auto &[piece, rest] : pairSplitImages(sizes))
    {
        auto page = ImageLink::fromUrl(imageUrls[piece]);
        if (rest >= 0)
            page.appendUrl = imageUrls[rest];
        pages.append(page);
    }

    int unknown = std::count_if(sizes.begin(), sizes.end(), [](const QSize &s) { return !s.isValid(); });
    qDebug() << "Page sizes probed in" << timer.elapsed() << "ms:" << imageUrls.size() << "images," << unknown
             << "unknown ->" << pages.size() << "pages";

    co_return pages;
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
