#include "mangakakalot.h"

#include "defines.h"
#include "downloadqueue.h"

Mangakakalot::Mangakakalot(QObject *parent, DownloadManager *dm)
    : AbstractMangaSource(parent, dm)
{
    name = "Mangakakalot";
    baseurl = "https://mangakakalot.com/";
    dicturl =
        "https://mangakakalot.com/"
        "manga_list?type=topview&category=all&state=all&page=";
}

MangaList Mangakakalot::getMangaList()
{
    QString rxstart(R"(<div class="main-wrapper">)");
    QString rxend(R"(<div class="panel_page_number">)");
    QRegularExpression mangarx(
        R"lit(<h3>\s*<a(?: rel="nofollow")? href="([^"]*)"\s*title="([^"]*)")lit");

    QRegularExpression nummangasrx("Total: ([0-9,]+)");
    QRegularExpression numpagesrx(R"(Last\(([0-9]+)\))");

    MangaList mangas;

    auto job = downloadmanager->downloadAsString(dicturl + "1");

    if (!job->await(2000))
    {
        emit updateError(job->errorString);
        return mangas;
    }

    emit updateProgress(30);

    QElapsedTimer timer;
    timer.start();

    auto nummangasrxmatch = nummangasrx.match(job->buffer);
    auto numpagesrxmatch = numpagesrx.match(job->buffer);

    mangas.nominalSize = 0;
    if (nummangasrxmatch.hasMatch())
        mangas.nominalSize = nummangasrxmatch.captured(1).remove(',').toInt();

    int pages = 1;
    if (nummangasrxmatch.hasMatch())
        pages = numpagesrxmatch.captured(1).toInt();

    QList<QString> urls;
    for (int i = 0; i < pages; i++) urls.append(dicturl + QString::number(i));

    DownloadQueue queue(downloadmanager, urls, maxparalleldownloads,
                        [&](QSharedPointer<DownloadStringJob> job) {
                            int spos = job->buffer.indexOf(rxstart);
                            int epos = job->buffer.indexOf(rxend);

                            int matches = 0;
                            for (auto &match : getAllRxMatches(
                                     mangarx, job->buffer, spos, epos))
                            {
                                mangas.links.append(match.captured(1));
                                mangas.titles.append(htmlToPlainText(
                                    htmlToPlainText(match.captured(2))));
                                matches++;
                            }
                            mangas.actualSize += matches;

                            emit updateProgress(100 * matches / pages);
                            qDebug() << "matches:" << matches;
                        });

    queue.start();
    awaitSignal(&queue, {SIGNAL(allCompleted())}, 1000000);

    mangalist.absoluteUrls = true;

    qDebug() << "mangas:" << mangas.actualSize << "time:" << timer.elapsed();

    emit updateProgress(100);

    return mangas;
}

void Mangakakalot::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression titlerx(R"(<ul class="manga-info-text">(.*?)</h1>)",
                               QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression authorrx(R"(Author\(s\) :(.*?)</li>)",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression artistrx;
    QRegularExpression statusrx("Status : ([^<]*)<");
    QRegularExpression yearrx;
    QRegularExpression genresrx(R"(Genres :[^>]*>(.*?)</li>)",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression summaryrx(
        R"lit(<meta name="description" content="([^"]*)")lit");
    QRegularExpression coverrx(
        R"lit(<meta name="twitter:image" content="([^"]*)")lit");

    QRegularExpression chapterrx(R"lit(<a href="([^"]*)"[^>]*>([^<]*)<)lit");

    fillMangaInfo(info, job->buffer, titlerx, authorrx, artistrx, statusrx,
                  yearrx, genresrx, summaryrx, coverrx);

    int spos = job->buffer.indexOf(R"(<div class="chapter-list">)");
    int epos = job->buffer.indexOf(R"(<div class="comment-info">)", spos);

    for (auto &chapterrxmatch :
         getAllRxMatches(chapterrx, job->buffer, spos, epos))
    {
        info->chapters.insert(0,
                              MangaChapter(chapterrxmatch.captured(1), this));

        QString ctitle = chapterrxmatch.captured(2);
        info->chapertitlesreversed.append(ctitle);
        info->numchapters++;
    }
}

QStringList Mangakakalot::getPageList(const QString &chapterlink)
{
    QRegularExpression pagerx(R"lit(<img src="([^"]*)")lit");

    auto job = downloadmanager->downloadAsString(chapterlink);

    QStringList pageLinks;

    if (!job->await(3000))
        return pageLinks;

    int spos = job->buffer.indexOf(R"(<div class="vung-doc" id="vungdoc">)");
    int epos = job->buffer.indexOf("</div>", spos);

    for (auto &match : getAllRxMatches(pagerx, job->buffer, spos, epos))
    {
        pageLinks.append(match.captured(1));
    }

    return pageLinks;
}

QString Mangakakalot::getImageLink(const QString &pagelink)
{
    // pagelinks are actually already imagelinks
    return pagelink;
}
