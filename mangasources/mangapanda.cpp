#include "mangapanda.h"

#include "defines.h"

MangaPanda::MangaPanda(QObject *parent, DownloadManager *dm)
    : AbstractMangaSource(parent, dm)
{
    name = "MangaPanda";
    baseurl = "http://www.mangapanda.com";
}

MangaList MangaPanda::getMangaList()
{
    QRegularExpression mangarx(R"lit(<li><a href="([^"]*)"[^>]*>([^<]*))lit");

    MangaList mangas;

    auto job = downloadManager->downloadAsString(baseurl + "/alphabetical");

    if (!job->await(8000))
    {
        emit updateError(job->errorString);
        return mangas;
    }

    emit updateProgress(30);

    QElapsedTimer timer;
    timer.start();

    int spos = job->buffer.indexOf(R"(<div class="series_col">)");
    int epos = job->buffer.indexOf(R"(<div id="adfooter">)");

    for (auto &match : getAllRxMatches(mangarx, job->buffer, spos, epos))
    {
        mangas.links.append(match.captured(1));
        mangas.titles.append(htmlToPlainText(match.captured(2)));
        mangas.actualSize++;
    }
    mangas.nominalSize = mangas.actualSize;

    qDebug() << "mangas:" << mangas.actualSize << "time:" << timer.elapsed();

    emit updateProgress(100);

    return mangas;
}

void MangaPanda::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression titlerx(R"(<h2 class="aname">([^<]*))");
    QRegularExpression authorrx("Author:</td>[^>]*>([^<]*)");
    QRegularExpression artistrx("Artist:</td>[^>]*>([^<]*)");
    QRegularExpression statusrx("Status:</td>[^>]*>([^<]*)");
    QRegularExpression yearrx("Year of Release:</td>[^>]*>([^<]*)");
    QRegularExpression genresrx("Genre:</td>(.*?)</td>",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression summaryrx(
        R"(<div id="readmangasum">.*?<p>(.*?)</p>)",
        QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression coverrx(R"(<div id="mangaimg"><img src="([^"]*))");

    QRegularExpression chapterrx(
        R"lit(<a href="([^"]*)"[^>]*>([^<]*)</a>([^<]*))lit");

    fillMangaInfo(info, job->buffer, titlerx, authorrx, artistrx, statusrx,
                  yearrx, genresrx, summaryrx, coverrx);

    int spos = job->buffer.indexOf(R"(<div id="chapterlist">)");
    int epos = job->buffer.indexOf(R"(<div id="adfooter">)", spos);

    for (auto &chapterrxmatch :
         getAllRxMatches(chapterrx, job->buffer, spos, epos))
    {
        info->chapters.append(
            MangaChapter(baseurl + chapterrxmatch.captured(1), this));

        QString ctitle = chapterrxmatch.captured(2);
        if (chapterrxmatch.captured(3) != " : ")
            ctitle += chapterrxmatch.captured(3);
        info->chaperTitleListDescending.insert(0, ctitle);
        info->numChapters++;
    }
}

QStringList MangaPanda::getPageList(const QString &chapterlink)
{
    QRegularExpression pagerx(R"lit(<option value="([^"]*)")lit");

    auto job = downloadManager->downloadAsString(chapterlink);
    QStringList pageLinks;

    if (!job->await(3000))
        return pageLinks;

    int spos = job->buffer.indexOf(R"(<select id="pageMenu")");
    int epos = job->buffer.indexOf("</select>", spos);

    for (auto &match : getAllRxMatches(pagerx, job->buffer, spos, epos))
    {
        pageLinks.append(baseurl + match.captured(1));
    }

    return pageLinks;
}

QString MangaPanda::getImageLink(const QString &pagelink)
{
    auto job = downloadManager->downloadAsString(pagelink);
    QString imageLink;

    QRegularExpression imagerx(R"lit(<img id="img"[^>]*src="([^"]*)")lit");

    if (!job->await(3000))
        return imageLink;

    auto match = imagerx.match(job->buffer);

    if (match.hasMatch())
        imageLink = match.captured(1);

    return imageLink;
}
