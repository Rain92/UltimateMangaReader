#include "mangahub.h"

MangaHub::MangaHub(NetworkManager *dm) : AbstractMangaSource(dm)
{
    name = "MangaHub";
    baseurl = "https://mangahub.io/";
    dicturl = "https://mangahub.io/search/page/";
}

bool MangaHub::uptareMangaList(UpdateProgressToken *token)
{
    QRegularExpression mangarx(R"lit(<a href="(https://mangahub.io/manga/[^"]+)">([^<]+)</a)lit");

    auto job = networkManager->downloadAsString(dicturl + "1");

    if (!job->await(7000))
    {
        token->sendError(job->errorString);
        return true;
    }

    token->sendProgress(10);

    QElapsedTimer timer;
    timer.start();

    MangaList mangas;
    mangas.absoluteUrls = true;

    int pages = 950;

    const int matchesPerPage = 30;
    int noMatchCounter = 0;

    auto lambda = [&](QSharedPointer<DownloadStringJob> job) {
        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, job->buffer))
        {
            auto title = htmlToPlainText(match.captured(2));
            auto url = match.captured(1);
            mangas.append(title, url);
            matches++;
        }

        if (matches == 0)
            noMatchCounter++;

        token->sendProgress(10 + 90 * (mangas.size / matchesPerPage) / pages);

        qDebug() << "matches:" << matches;
    };

    lambda(job);

    int oldPages = 1;
    while (noMatchCounter < 2 && pages < 2000)
    {
        qDebug() << "pages:" << pages;

        QList<QString> urls;
        for (int i = oldPages + 1; i <= pages; i++)
            urls.append(dicturl + QString::number(i));

        DownloadQueue queue(networkManager, urls, CONF.parallelDownloadsMid, lambda, true);
        queue.setCancellationToken(&token->canceled);
        queue.start();
        if (!queue.awaitCompletion())
        {
            token->sendError(queue.lastErrorMessage);
            return false;
        }
        oldPages = pages;
        pages += 50;
    }

    this->mangaList = mangas;

    qDebug() << "mangas:" << mangas.size << "time:" << timer.elapsed();

    token->sendProgress(100);

    return true;
}

void MangaHub::updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                              QSharedPointer<MangaInfo> info)
{
    QRegularExpression titlerx(R"(<li class="active"><span>([^<]*)<)");
    QRegularExpression authorrx(R"(Author</span><span>([^<]*)</span>)");
    QRegularExpression artistrx(R"(Artist</span><span>([^<]*)</span>)");
    QRegularExpression statusrx(R"(Status</span><span>([^<]*)</span>)");
    QRegularExpression yearrx;
    QRegularExpression genresrx(R"(genre-label">(.*?)</div>)");
    QRegularExpression summaryrx(R"lit(<meta name="description" content="([^"]*)")lit");
    QRegularExpression coverrx(R"lit(<meta property="og:image" content="([^"]*)")lit");

    QRegularExpression chapterrx(
        R"lit(<a href="(https://mangahub.io/chapter/[^"]+)"[^>]*>(.*?)</span></span>)lit");

    for (auto &c : job->getCookies())
        networkManager->addCookie(c.domain(), c.name(), c.value());

    fillMangaInfo(info, job->buffer, titlerx, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx,
                  coverrx);

    // fix genres spacing
    for (int i = 1; i < info->genres.size(); i++)
    {
        if (info->genres[i].isUpper())
        {
            info->genres.insert(i, ' ');
            i++;
        }
    }

    auto spos = job->buffer.indexOf(R"(<div class="tab-content">)");
    auto epos = job->buffer.indexOf(R"(<section id="comments">)");

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->buffer, spos, epos))
        newchapters.insert(
            0, MangaChapter(htmlToPlainText(chapterrxmatch.captured(2)), chapterrxmatch.captured(1)));
    info->chapters.mergeChapters(newchapters);
}

int MangaHub::binarySearchNumPages(const QRegularExpressionMatch &imagerxmatch, int lowerBound,
                                   int upperBound, bool upperChecked)
{
    if (!upperChecked)
    {
        bool valid = networkManager->urlExists(buildImgUrl(imagerxmatch, upperBound));
        if (valid)
            return binarySearchNumPages(imagerxmatch, lowerBound, upperBound * 2, false);
    }

    if (upperBound - lowerBound == 1)
        return lowerBound;

    int mid = (lowerBound + upperBound + 1) / 2;

    bool valid = networkManager->urlExists(buildImgUrl(imagerxmatch, mid));
    if (valid)
        return binarySearchNumPages(imagerxmatch, mid, upperBound, true);
    else
        return binarySearchNumPages(imagerxmatch, lowerBound, mid, true);
}

Result<QStringList, QString> MangaHub::getPageList(const QString &chapterUrl)
{
    QRegularExpression imagerx(R"lit(<img src="([^"]+?/)\d+(\..{3,4})")lit");

    QRegularExpression numimagesrx(R"lit(>1/(\d+)<)lit");

    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    auto imagerxmatch = imagerx.match(job->buffer);
    auto numimagesrxmatch = numimagesrx.match(job->buffer);

    if (!imagerxmatch.hasMatch())
        return Err(QString("Error. Couldn't process pages/images."));

    int pages = 1;
    if (numimagesrxmatch.hasMatch())
    {
        pages = numimagesrxmatch.captured(1).toInt();
    }
    else
    {
        pages = binarySearchNumPages(imagerxmatch, 1, 25, false);
    }

    QStringList imageUrls;
    for (int i = 1; i <= pages; i++)
    {
        auto url = buildImgUrl(imagerxmatch, i);
        imageUrls.append(url);
    }

    return Ok(imageUrls);
}
