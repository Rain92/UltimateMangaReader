#include "mangahub.h"

#include "mangaowl.h"

MangaHub::MangaHub(QObject *parent, DownloadManager *dm)
    : AbstractMangaSource(parent, dm)
{
    name = "MangaHub";
    baseurl = "https://mangahub.io/";
    dicturl = "https://mangahub.io/search/page/";
}

MangaList MangaHub::getMangaList()
{
    QRegularExpression mangarx(
        R"lit(<a href="(https://mangahub.io/manga/[^"]+)">([^<]+)<)lit");

    MangaList mangas;

    auto job = downloadManager->downloadAsString(dicturl + "1");

    if (!job->await(2000))
    {
        emit updateError(job->errorString);
        return mangas;
    }

    emit updateProgress(10);

    QElapsedTimer timer;
    timer.start();

    int pages = 950;

    const int matchesPerPage = 36;
    int noMatchCounter = 0;

    auto lambda = [&](QSharedPointer<DownloadJobBase> job) {
        auto sjob = static_cast<DownloadStringJob *>(job.get());
        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, sjob->buffer))
        {
            mangas.links.append(match.captured(1));
            mangas.titles.append(
                htmlToPlainText(htmlToPlainText(match.captured(2))));
            matches++;
        }
        mangas.actualSize += matches;

        if (matches == 0)
            noMatchCounter++;

        emit updateProgress(10 +
                            90 * (mangas.actualSize / matchesPerPage) / pages);

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

        DownloadQueue queue(downloadManager, urls, 8, lambda);
        queue.start();
        awaitSignal(&queue, {SIGNAL(allCompleted())}, 1000000);

        oldPages = pages;
        pages += 50;
    }

    mangas.nominalSize = mangas.actualSize;
    mangas.absoluteUrls = true;

    qDebug() << "mangas:" << mangas.actualSize << "time:" << timer.elapsed();

    emit updateProgress(100);

    return mangas;
}

void MangaHub::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression titlerx(R"(<li class="active"><span>([^<]*)<)");
    QRegularExpression authorrx(R"(Author</span><span>([^<]*)</span>)");
    QRegularExpression artistrx(R"(Artist</span><span>([^<]*)</span>)");
    QRegularExpression statusrx(R"(Status</span><span>([^<]*)</span>)");
    QRegularExpression yearrx;
    QRegularExpression genresrx(R"(genre-label">(.*?)</div>)");
    QRegularExpression summaryrx(
        R"lit(<meta name="description" content="([^"]*)")lit");
    QRegularExpression coverrx(
        R"lit(<meta property="og:image" content="([^"]*)")lit");

    QRegularExpression chapterrx(
        R"lit(<a href="(https://mangahub.io/chapter/[^"]+)"[^>]*>(.*?)</span></span>)lit");

    for (auto &c : job->getCookies())
        downloadManager->addCookie(c.domain(), c.name(), c.value());

    fillMangaInfo(info, job->buffer, titlerx, authorrx, artistrx, statusrx,
                  yearrx, genresrx, summaryrx, coverrx);

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
    for (auto &chapterrxmatch :
         getAllRxMatches(chapterrx, job->buffer, spos, epos))
        newchapters.insert(
            0, MangaChapter(htmlToPlainText(chapterrxmatch.captured(2)),
                            chapterrxmatch.captured(1)));
    info->chapters.mergeChapters(newchapters);
}

int MangaHub::binarySearchNumPages(const QRegularExpressionMatch &imagerxmatch,
                                   int lowerBound, int upperBound,
                                   bool upperChecked)
{
    if (!upperChecked)
    {
        bool valid =
            downloadManager->urlExists(buildImgUrl(imagerxmatch, upperBound));
        if (valid)
            return binarySearchNumPages(imagerxmatch, lowerBound,
                                        upperBound * 2, false);
    }

    if (upperBound - lowerBound == 1)
        return lowerBound;

    int mid = (lowerBound + upperBound + 1) / 2;

    bool valid = downloadManager->urlExists(buildImgUrl(imagerxmatch, mid));
    if (valid)
        return binarySearchNumPages(imagerxmatch, mid, upperBound, true);
    else
        return binarySearchNumPages(imagerxmatch, lowerBound, mid, true);
}

QStringList MangaHub::getPageList(const QString &chapterlink)
{
    QRegularExpression imagerx(R"lit(<img src="([^"]+?/)\d+(\..{3,4})")lit");

    QRegularExpression numimagesrx(R"lit(>1/(\d+)<)lit");

    auto job = downloadManager->downloadAsString(chapterlink);

    QStringList pageLinks;

    if (!job->await(3000))
        return pageLinks;

    auto imagerxmatch = imagerx.match(job->buffer);
    auto numimagesrxmatch = numimagesrx.match(job->buffer);

    if (!imagerxmatch.hasMatch())
        return pageLinks;

    int pages = 1;
    if (numimagesrxmatch.hasMatch())
    {
        pages = numimagesrxmatch.captured(1).toInt();
    }
    else
    {
        pages = binarySearchNumPages(imagerxmatch, 1, 25, false);
    }

    for (int i = 1; i <= pages; i++)
    {
        auto link = buildImgUrl(imagerxmatch, i);
        pageLinks.append(link);
    }

    return pageLinks;
}

QString MangaHub::getImageLink(const QString &pagelink)
{
    // pagelinks are actually already imagelinks
    return pagelink;
}
