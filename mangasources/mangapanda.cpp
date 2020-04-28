#include "mangapanda.h"

MangaPanda::MangaPanda(NetworkManager *dm) : AbstractMangaSource(dm)
{
    name = "MangaPanda";
    baseurl = "http://www.mangapanda.com";
}

bool MangaPanda::uptareMangaList(UpdateProgressToken *token)
{
    QRegularExpression mangarx(R"lit(<li><a href="([^"]*)"[^>]*>([^<]*))lit");

    MangaList mangas;

    auto job = networkManager->downloadAsString(baseurl + "/alphabetical");

    if (!job->await(10000))
    {
        token->sendError(job->errorString);
        return false;
    }

    token->sendProgress(30);

    QElapsedTimer timer;
    timer.start();

    int spos = job->buffer.indexOf(R"(<div class="series_col">)");
    int epos = job->buffer.indexOf(R"(<div id="adfooter">)");

    for (auto &match : getAllRxMatches(mangarx, job->buffer, spos, epos))
    {
        mangas.urls.append(match.captured(1));
        mangas.titles.append(htmlToPlainText(match.captured(2)));
        mangas.size++;
    }

    qDebug() << "mangas:" << mangas.size << "time:" << timer.elapsed();

    token->sendProgress(100);

    return true;
}

void MangaPanda::updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                                QSharedPointer<MangaInfo> info)
{
    QRegularExpression titlerx(R"(<h2 class="aname">([^<]*))");
    QRegularExpression authorrx("Author:</td>[^>]*>([^<]*)");
    QRegularExpression artistrx("Artist:</td>[^>]*>([^<]*)");
    QRegularExpression statusrx("Status:</td>[^>]*>([^<]*)");
    QRegularExpression yearrx("Year of Release:</td>[^>]*>([^<]*)");
    QRegularExpression genresrx("Genre:</td>(.*?)</td>", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression summaryrx(R"(<div id="readmangasum">.*?<p>(.*?)</p>)",
                                 QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression coverrx(R"(<div id="mangaimg"><img src="([^"]*))");

    QRegularExpression chapterrx(R"lit(<a href="([^"]*)"[^>]*>([^<]*)</a>([^<]*))lit");

    fillMangaInfo(info, job->buffer, titlerx, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx,
                  coverrx);

    int spos = job->buffer.indexOf(R"(<div id="chapterlist">)");
    int epos = job->buffer.indexOf(R"(<div id="adfooter">)", spos);

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->buffer, spos, epos))
    {
        auto ctitle = chapterrxmatch.captured(2);
        if (chapterrxmatch.captured(3) != " : ")
            ctitle += chapterrxmatch.captured(3);
        auto curl = baseurl + chapterrxmatch.captured(1);
        newchapters.append(MangaChapter(ctitle, curl));
    }
    info->chapters.mergeChapters(newchapters);
}

Result<QStringList, QString> MangaPanda::getPageList(const QString &chapterUrl)
{
    QRegularExpression pagerx(R"lit(<option value="([^"]*)")lit");

    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    int spos = job->buffer.indexOf(R"(<select id="pageMenu")");
    int epos = job->buffer.indexOf("</select>", spos);

    QStringList pageUrls;
    for (auto &match : getAllRxMatches(pagerx, job->buffer, spos, epos))
    {
        pageUrls.append(baseurl + match.captured(1));
    }

    return Ok(pageUrls);
}

Result<QString, QString> MangaPanda::getImageUrl(const QString &pageUrl)
{
    QRegularExpression imagerx(R"lit(<img id="img"[^>]*src="([^"]*)")lit");

    auto job = networkManager->downloadAsString(pageUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    auto match = imagerx.match(job->buffer);

    if (!match.hasMatch())
        return Err(QString("Couldn't process pages/images."));

    auto imageUrl = match.captured(1);

    return Ok(imageUrl);
}
