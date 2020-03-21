#include "mangaowl.h"

MangaOwl::MangaOwl(QObject *parent, DownloadManager *dm)
    : AbstractMangaSource(parent, dm)
{
    name = "MangaOwl";
    baseurl = "https://mangaowl.net/";
    dicturl = "https://mangaowl.net/list/";
}

MangaList MangaOwl::getMangaList()
{
    QRegularExpression numpagesrx(
        R"(>(\d+)</a>\s*</li>\s*<li>\s*<a[^>]*?rel="next")");

    QRegularExpression mangarx(
        R"lit(<a href="(https://mangaowl.com/single/[^"]+)">.*?</td>.*?<td>([^<]+)</td>)lit",
        QRegularExpression::DotMatchesEverythingOption);

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

    auto numpagesrxmatch = numpagesrx.match(job->buffer);

    int pages = 1;
    if (numpagesrxmatch.hasMatch())
        pages = numpagesrxmatch.captured(1).toInt();

    qDebug() << "pages:" << pages;

    const int matchesPerPage = 36;

    auto lambda = [&](QSharedPointer<DownloadStringJob> job) {
        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, job->buffer))
        {
            mangas.links.append(match.captured(1));
            mangas.titles.append(
                htmlToPlainText(htmlToPlainText(match.captured(2))));
            matches++;
        }
        mangas.actualSize += matches;

        emit updateProgress(10 +
                            90 * (mangas.actualSize / matchesPerPage) / pages);

        qDebug() << "matches:" << matches;
    };

    lambda(job);

    QList<QString> urls;
    for (int i = 2; i <= pages; i++)
        urls.append(dicturl + QString::number(i));

    DownloadQueue queue(downloadManager, urls, 8, lambda);

    queue.start();
    awaitSignal(&queue, {SIGNAL(allCompleted())}, 1000000);

    mangas.nominalSize = mangas.actualSize;
    mangas.absoluteUrls = true;

    qDebug() << "mangas:" << mangas.actualSize << "time:" << timer.elapsed();

    emit updateProgress(100);

    return mangas;
}

void MangaOwl::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression titlerx(
        R"lit(<img class="lozad"[^>]*title="([^"]*)")lit");
    QRegularExpression authorrx(R"(Author.*?<a[^>]*>\s*(.*?)\s*</a>)",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression artistrx;
    QRegularExpression statusrx(R"(Pub. status.*?</span>\s*(.*?)\s*</p>)",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression yearrx(R"(Released.*?</span>\s*(.*?)\s*</p>)",
                              QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression genresrx(R"(<span>Genres.*?<p>(.*?)<span>)",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression summaryrx(
        R"lit(Story Line.*?</span>(.*?)</div>)lit",
        QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression coverrx(
        R"lit(<img class="lozad"[^>]*data-src="([^"]*)")lit");

    QRegularExpression chapterrx(
        R"lit(<a[^>]*class="chapter-url"[^>]*href="([^"]*)"[^>]*>\s*<label>\s*(.*?)\s*</label>)lit",
        QRegularExpression::DotMatchesEverythingOption);

    fillMangaInfo(info, job->buffer, titlerx, authorrx, artistrx, statusrx,
                  yearrx, genresrx, summaryrx, coverrx);

    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->buffer))
    {
        info->chapters.insert(0,
                              MangaChapter(chapterrxmatch.captured(1), this));

        QString ctitle = chapterrxmatch.captured(2);
        info->chaperTitleListDescending.append(ctitle);
        info->numChapters++;
    }
}

QStringList MangaOwl::getPageList(const QString &chapterlink)
{
    QRegularExpression pagerx(
        R"lit(<img[^>]*class="owl-lazy"[^>]*data-src="([^"]*)")lit");

    auto job = downloadManager->downloadAsString(chapterlink);

    QStringList pageLinks;

    if (!job->await(3000))
        return pageLinks;

    for (auto &match : getAllRxMatches(pagerx, job->buffer))
    {
        pageLinks.append(match.captured(1));
    }

    return pageLinks;
}

QString MangaOwl::getImageLink(const QString &pagelink)
{
    // pagelinks are actually already imagelinks
    return pagelink;
}
