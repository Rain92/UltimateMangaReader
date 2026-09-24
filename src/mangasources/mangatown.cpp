#include "mangatown.h"

static QString joinLinkTexts(const QString &html)
{
    QRegularExpression linkrx(R"lit(<a[^>]*>([^<]*)</a>)lit");

    QStringList names;
    for (auto &match : getAllRxMatches(linkrx, html))
    {
        auto name = match.captured(1).trimmed();
        if (!name.isEmpty() && !names.contains(name))
            names.append(name);
    }

    return names.isEmpty() ? html : names.join(", ");
}

static QString absoluteImageUrl(const QString &url)
{
    if (url.startsWith("//"))
        return "https:" + url;

    return url;
}

MangaTown::MangaTown(NetworkManager *networkManager) : AbstractMangaSource(networkManager)
{
    name = "MangaTown";
    baseUrl = "https://www.mangatown.com";
    dictionaryUrl = "https://www.mangatown.com/directory/";
    // info pages of long series are large (~350 KB) and the server sometimes takes > 1 s to answer
    mangaInfoTimeout = 6000;

    // image hosts: zjcdn.mangahere.org (pages), fmcdn.mangahere.com (covers)
    networkManager->addSetCustomRequestHeader("mangahere.org", "Referer", R"(https://www.mangatown.com/)");
    networkManager->addSetCustomRequestHeader("mangahere.com", "Referer", R"(https://www.mangatown.com/)");
}

QString MangaTown::licensedMessage(const QString &buffer)
{
    QRegularExpression licensedrx(R"lit(>([^<>]*has been licensed[^<>]*)<)lit");

    auto match = licensedrx.match(buffer);
    if (match.hasMatch())
        return htmlToPlainText(match.captured(1)).trimmed();

    return "";
}

bool MangaTown::updateMangaList(UpdateProgressToken *token)
{
    QRegularExpression mangarx(R"lit(<a class="manga_cover" href="(/manga/[^"]*?)" title="(.*?)">)lit");
    QRegularExpression viewsrx(R"lit(Views:\s*(\d+))lit");

    const int matchesPerPage = 30;
    const int parallelDownloads = CONF.parallelDownloadsHigh;

    // the pagination links stop at page 334 but the pages behind them are served as well (~800 pages in 2026),
    // so keep requesting pages while they are full of new mangas.
    // A-Z order is stable while crawling, the popularity comes from the view counts.
    const int estimatedPages = qMax(800, (mangaList.size + matchesPerPage - 1) / matchesPerPage);
    const int maxPages = 3 * estimatedPages;

    QList<QSharedPointer<DownloadStringJob>> jobs;
    int nextPage = 1;
    auto requestNextPage = [&]()
    {
        if (nextPage > maxPages)
            return;
        auto url = QString("%1%2.htm?name.az").arg(dictionaryUrl).arg(nextPage++);
        jobs.append(networkManager->downloadAsString(url, 30000));
    };

    for (int i = 0; i < parallelDownloads; i++)
        requestNextPage();

    QElapsedTimer timer;
    timer.start();

    MangaList mangas;
    mangas.absoluteUrls = false;
    QSet<QString> urls;
    QList<qint64> views;

    // short pages are the end of the directory, unless a full page follows them
    QStringList shortPages;
    QSet<QString> refetchedPages;

    for (int page = 1; !jobs.isEmpty(); page++)
    {
        auto job = jobs.takeFirst();

        if (!job->isCompleted && job->errorString != "")
            job->restart();

        if (!job->await(30000))
        {
            token->sendError(job->errorString);
            return false;
        }

        if (token->canceled)
        {
            token->sendError(QString("Download cancelled"));
            return false;
        }

        int oldSize = mangas.size;
        auto matches = getAllRxMatches(mangarx, job->bufferStr);
        for (int i = 0; i < matches.size(); i++)
        {
            auto url = matches[i].captured(1);
            if (urls.contains(url))
                continue;
            urls.insert(url);

            int epos = i + 1 < matches.size() ? matches[i + 1].capturedStart() : job->bufferStr.length();
            auto viewsmatch = viewsrx.match(job->bufferStr, matches[i].capturedEnd());
            bool hasViews = viewsmatch.hasMatch() && viewsmatch.capturedStart() < epos;

            mangas.append(htmlToPlainText(matches[i].captured(2)).trimmed(), url);
            views.append(hasViews ? viewsmatch.captured(1).toLongLong() : 0);
        }

        if (matches.size() >= matchesPerPage && mangas.size > oldSize)
        {
            requestNextPage();

            // the short pages before this one weren't the end: fetch them once more
            for (const auto &url : std::as_const(shortPages))
            {
                refetchedPages.insert(url);
                jobs.append(networkManager->downloadAsString(url, 30000));
            }
            shortPages.clear();
        }
        else
        {
            qDebug() << "Incomplete page:" << job->originalUrl << matches.size() << mangas.size - oldSize;
            if (!refetchedPages.contains(job->originalUrl))
                shortPages.append(job->originalUrl);
        }

        token->sendProgress(qMin(99, 10 + 90 * page / estimatedPages));
    }

    if (mangas.size == 0)
    {
        token->sendError(QString("Couldn't parse manga list."));
        return false;
    }

    QVector<int> order(mangas.size);
    for (int i = 0; i < mangas.size; i++)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&views](int a, int b) { return views[a] > views[b]; });
    for (int rank = 0; rank < mangas.size; rank++)
        mangas.popularityRanks[order[rank]] = rank;

    this->mangaList = mangas;

    qDebug() << "pages:" << nextPage - 1 << "refetched:" << refetchedPages.size() << "mangas:" << mangas.size
             << "time:" << timer.elapsed();

    token->sendProgress(100);

    return true;
}

Result<MangaChapterCollection, QString> MangaTown::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression authorrx(R"lit(<b>Author\(s\):</b>(.*?)</?li)lit",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression artistrx(R"lit(<b>Artist\(s\):</b>(.*?)</?li)lit",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression statusrx(R"lit(<b>Status\(s\):</b>\s*([^<&]*))lit");
    QRegularExpression yearrx;
    QRegularExpression genresrx(R"lit(<b>Genre\(s\):</b>(.*?)</li>)lit",
                                QRegularExpression::DotMatchesEverythingOption);

    QRegularExpression summaryrx(
        R"lit(<span id="show"[^>]*>(.*?)(?:&nbsp;)?\s*(?:<a class="more"[^>]*>[^<]*</a>\s*)?</span>)lit",
        QRegularExpression::DotMatchesEverythingOption);

    QRegularExpression coverrx(R"lit(<div class="detail_info[^"]*">\s*<img src="([^"]*)")lit");

    QRegularExpression chapterrx(R"lit(<li>(.*?)</li>)lit", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression chapterlinkrx(R"lit(<a href="(/manga/[^"]*)"[^>]*>([^<]*)</a>)lit");
    QRegularExpression chapterspanrx(R"lit(<span>([^<]*)</span>)lit");
    QRegularExpression volumerx(R"lit(^Vol\s+\S+$)lit");

    if (!job->bufferStr.contains(R"(class="detail_info)"))
        return Err(QString("Couldn't parse manga infos."));

    fillMangaInfo(info, job->bufferStr, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx, coverrx);

    auto authorrxmatch = authorrx.match(job->bufferStr);
    if (authorrxmatch.hasMatch())
        info->author = htmlToPlainText(joinLinkTexts(authorrxmatch.captured(1))).trimmed();
    auto artistrxmatch = artistrx.match(job->bufferStr);
    if (artistrxmatch.hasMatch())
        info->artist = htmlToPlainText(joinLinkTexts(artistrxmatch.captured(1))).trimmed();
    info->status = info->status.trimmed();
    info->summary = info->summary.trimmed();
    info->coverUrl = absoluteImageUrl(info->coverUrl);

    int spos = job->bufferStr.indexOf(R"(<ul class="chapter_list">)");
    if (spos == -1)
    {
        auto message = licensedMessage(job->bufferStr);
        if (message != "")
            return Err(message);

        return Ok(MangaChapterCollection());
    }
    int epos = job->bufferStr.indexOf(R"(</ul>)", spos);

    MangaChapterCollection newchapters;
    QSet<QString> chapterUrls;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->bufferStr, spos, epos))
    {
        auto li = chapterrxmatch.captured(1);
        auto linkmatch = chapterlinkrx.match(li);
        if (!linkmatch.hasMatch())
            continue;

        auto url = baseUrl + linkmatch.captured(1);
        if (chapterUrls.contains(url))
            continue;
        chapterUrls.insert(url);

        QString volume;
        QStringList subtitles;
        for (auto &spanmatch : getAllRxMatches(chapterspanrx, li))
        {
            auto text = htmlToPlainText(spanmatch.captured(1)).trimmed();
            if (volume.isEmpty() && subtitles.isEmpty() && volumerx.match(text).hasMatch())
                volume = text;
            else if (!text.isEmpty())
                subtitles.append(text);
        }

        auto title = htmlToPlainText(linkmatch.captured(2)).trimmed();
        if (!volume.isEmpty())
            title = volume + " " + title;
        if (!subtitles.isEmpty())
            title += ": " + subtitles.join(" ");

        newchapters.insert(0, MangaChapter(title, url));
    }

    return Ok(newchapters);
}

Result<QStringList, QString> MangaTown::getPageList(const QString &chapterUrl)
{
    QRegularExpression totalpagesrx(R"lit(var total_pages\s*=\s*(\d+))lit");
    QRegularExpression numPagesRx(
        R"lit(>(\d+)</option>\s*?(?:<option\s*?value="/manga/[^"]*?"\s*?>Featured</option>)?\s*?</select>)lit");
    QRegularExpression stripimagerx(R"lit(<img src="([^"]*)" class="image")lit");

    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    int vpos = job->bufferStr.indexOf(R"(<div id="viewer")");
    if (vpos == -1)
    {
        auto message = licensedMessage(job->bufferStr);
        if (message != "")
            return Err(message);

        return Err(QString("Couldn't process pagelist."));
    }

    QStringList imageUrls;

    // long strip chapters show all images on one page
    int epos = job->bufferStr.indexOf(R"(<div class="manga_read)", vpos);
    for (auto &match : getAllRxMatches(stripimagerx, job->bufferStr, vpos, epos))
        imageUrls.append(absoluteImageUrl(match.captured(1)));

    if (!imageUrls.isEmpty())
        return Ok(imageUrls);

    int numPages = 0;
    auto totalpagesmatch = totalpagesrx.match(job->bufferStr);
    if (totalpagesmatch.hasMatch())
        numPages = totalpagesmatch.captured(1).toInt();

    if (numPages <= 0)
    {
        auto numPagesRxMatch = numPagesRx.match(job->bufferStr);
        if (numPagesRxMatch.hasMatch())
            numPages = numPagesRxMatch.captured(1).toInt();
    }

    if (numPages <= 0)
        return Err(QString("Couldn't process pagelist."));

    auto baseChapterUrl = job->url;
    if (!baseChapterUrl.endsWith('/'))
        baseChapterUrl += '/';

    for (int i = 1; i <= numPages; i++)
        imageUrls.append(QString("%1%2.html").arg(baseChapterUrl).arg(i));

    return Ok(imageUrls);
}

Result<QString, QString> MangaTown::getImageUrl(const QString &pageUrl)
{
    if (!pageUrl.endsWith(".html"))
        return Ok(pageUrl);

    QRegularExpression imgUrlRx(R"lit(<img src="([^"]*)"[^>]*id="image")lit");
    QRegularExpression anyImgUrlRx(R"lit(<img[^>]*src="([^"]*)")lit");

    auto job = networkManager->downloadAsString(pageUrl);

    if (!job->await(6000))
        return Err(job->errorString);

    int vpos = qMax(0, job->bufferStr.indexOf(R"(<div id="viewer")"));

    auto match = imgUrlRx.match(job->bufferStr, vpos);
    if (!match.hasMatch())
        match = anyImgUrlRx.match(job->bufferStr, vpos);

    if (!match.hasMatch())
        return Err(QString("Couldn't process pages/images."));

    return Ok(absoluteImageUrl(match.captured(1)));
}
