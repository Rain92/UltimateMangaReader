#include "mangakakalot.h"

Mangakakalot::Mangakakalot(NetworkManager *dm) : AbstractMangaSource(dm)
{
    name = "Mangakakalot";
    baseurl = "https://mangakakalot.com/";
    dictionaryUrl =
        "https://mangakakalot.com/"
        "manga_list?type=topview&category=all&state=all&page=";
}

bool Mangakakalot::uptareMangaList(UpdateProgressToken *token)
{
    QString rxstart(R"(<div class="main-wrapper">)");
    QString rxend(R"(<div class="panel_page_number">)");
    QRegularExpression mangarx(R"lit(<h3>\s*<a(?: rel="nofollow")? href="([^"]*)"\s*title="([^"]*)")lit");

    QRegularExpression nummangasrx("Total: ([0-9,]+)");
    QRegularExpression numpagesrx(R"(Last\(([0-9]+)\))");

    auto job = networkManager->downloadAsString(dictionaryUrl + "1");

    if (!job->await(7000))
    {
        token->sendError(job->errorString);
        return false;
    }

    token->sendProgress(10);

    QElapsedTimer timer;
    timer.start();

    auto nummangasrxmatch = nummangasrx.match(job->buffer);
    auto numpagesrxmatch = numpagesrx.match(job->buffer);

    int nominalSize = 0;
    if (nummangasrxmatch.hasMatch())
        nominalSize = nummangasrxmatch.captured(1).remove(',').toInt();

    MangaList mangas;
    mangas.absoluteUrls = true;

    int pages = 1;
    if (numpagesrxmatch.hasMatch())
        pages = numpagesrxmatch.captured(1).toInt();

    const int matchesPerPage = 24;
    auto lambda = [&](QSharedPointer<DownloadJobBase> job) {
        auto sjob = static_cast<DownloadStringJob *>(job.get());

        int spos = sjob->buffer.indexOf(rxstart);
        int epos = sjob->buffer.indexOf(rxend);

        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, sjob->buffer, spos, epos))
        {
            mangas.urls.append(match.captured(1));
            mangas.titles.append(htmlToPlainText(htmlToPlainText(match.captured(2))));
            matches++;
        }
        mangas.size += matches;

        token->sendProgress(10 + 90 * (mangas.size / matchesPerPage) / pages);
        qDebug() << "matches:" << matches;
    };

    if (nominalSize != mangas.size)
        qDebug() << "Not all mangas captured:" << nominalSize << "vs" << mangas.size;

    lambda(job);

    QList<QString> urls;
    for (int i = 2; i <= pages; i++)
        urls.append(dictionaryUrl + QString::number(i));

    DownloadQueue queue(networkManager, urls, CONF.parallelDownloadsHigh, lambda, true);
    queue.setCancellationToken(&token->canceled);
    queue.start();
    if (!queue.awaitCompletion())
    {
        token->sendError(queue.lastErrorMessage);
        return false;
    }
    this->mangaList = mangas;

    qDebug() << "mangas:" << mangas.size << "time:" << timer.elapsed();

    token->sendProgress(100);

    return true;
}
void Mangakakalot::updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                                  QSharedPointer<MangaInfo> info)
{
    QRegularExpression titlerx(R"(<ul class="manga-info-text">(.*?)</h1>)",
                               QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression authorrx(R"(author/[^>]*?>([^<]*?)<)");
    QRegularExpression artistrx;
    QRegularExpression statusrx(R"(Status :(?:\s|</td>\s)(.*?)(?:</li>|</td>))",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression yearrx;
    QRegularExpression genresrx(R"(Genres :[^>]*>(.*?)(?:</li>|</tr>))",
                                QRegularExpression::DotMatchesEverythingOption);

    QRegularExpression summaryrx(R"lit(<meta name="description" content="([^"]*)")lit");

    QRegularExpression coverrx(R"lit(<meta name="twitter:image" content="([^"]*)")lit");

    QRegularExpression chapterrx(R"lit(<a[^>]*?href="([^"]*)"[^>]*>([^<]*)<)lit");

    fillMangaInfo(info, job->buffer, titlerx, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx,
                  coverrx);

    info->genres = info->genres.remove("- ");
    info->status = info->status.remove('\n');
    if (info->status.contains('-'))
        info->status = info->status.split('-')[0];

    int spos = job->buffer.indexOf(R"(chapter-list">)");
    int epos = job->buffer.indexOf(R"(<div class="fb-comments)", spos);

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->buffer, spos, epos))
        newchapters.insert(0, MangaChapter(chapterrxmatch.captured(2), chapterrxmatch.captured(1)));
    info->chapters.mergeChapters(newchapters);
}

Result<QStringList, QString> Mangakakalot::getPageList(const QString &chapterUrl)
{
    QRegularExpression pagerx(R"lit(<img src="([^"]*)")lit");

    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    int spos = job->buffer.indexOf(R"(<div class="vung-doc" id="vungdoc">)");
    if (spos < 0)
        spos = job->buffer.indexOf(R"(<div class="container-chapter-reader">)");
    int epos = job->buffer.indexOf("</div>", spos);

    QStringList imageUrls;
    for (auto &match : getAllRxMatches(pagerx, job->buffer, spos, epos))
    {
        imageUrls.append(match.captured(1));
    }

    return Ok(imageUrls);
}
