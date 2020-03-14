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
    QRegularExpression rx(R"lit(<li><a href="([^"]*)"[^>]*>([^<]*))lit");

    MangaList mangas;

    auto job = downloadmanager->downloadAsString(baseurl + "/alphabetical");

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

    for (auto &match : getAllRxMatches(rx, job->buffer, spos, epos))
    {
        mangas.links.append(match.captured(1));
        mangas.titles.append(htmlToPlainText(match.captured(2)));
        mangas.actualSize++;
    }
    mangas.nominalSize = mangas.actualSize;

    auto time = timer.nsecsElapsed() / 1000;

    qDebug() << mangas.actualSize << "time:" << time;

    emit updateProgress(100);

    return mangas;
}

QSharedPointer<MangaInfo> MangaPanda::getMangaInfo(const QString &mangalink)
{
    auto job = downloadmanager->downloadAsString(mangalink);

    auto info = QSharedPointer<MangaInfo>(new MangaInfo(this));

    info->mangasource = this;
    info->hostname = name;

    info->link = mangalink;

    if (!job->await(3000))
        return info;

    updateMangaInfoFinishedLoading(job, info);

    downloadCover(info);

    return info;
}

void MangaPanda::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression titlerx(R"(<h2 class="aname">([^<]*))");
    QRegularExpression authorrx("Author:</td>[^>]*>([^<]*)");
    QRegularExpression artistrx("Artist:</td>[^>]*>([^<]*)");
    QRegularExpression statusrx("Status:</td>[^>]*>([^<]*)");
    QRegularExpression yearrx("Year of Release:</td>[^>]*>([^<]*)");
    QRegularExpression genresrx("Genre:</td>(.*)</td>");
    QRegularExpression summaryrx(R"(<div id="readmangasum">.*<p>([^<]*)</p>)");
    QRegularExpression coverrx(R"(<div id="mangaimg"><img src="([^"]*))");

    QRegularExpression chapterrx(
        R"lit(<a href="([^"]*)"[^>]*>([^<]*)</a>([^<]*))lit");

    auto titlerxmatch = titlerx.match(job->buffer);
    auto authorrxmatch = authorrx.match(job->buffer);
    auto artistrxmatch = artistrx.match(job->buffer);
    auto statusrxmatch = statusrx.match(job->buffer);
    auto yearrxmatch = yearrx.match(job->buffer);
    auto genresrxmatch = genresrx.match(job->buffer);
    auto summaryrxmatch = summaryrx.match(job->buffer);
    auto coverrxmatch = coverrx.match(job->buffer);

    if (titlerxmatch.hasMatch())
        info->title = htmlToPlainText(titlerxmatch.captured(1));
    if (authorrxmatch.hasMatch())
        info->author = authorrxmatch.captured(1);
    if (artistrxmatch.hasMatch())
        info->artist = artistrxmatch.captured(1);
    if (statusrxmatch.hasMatch())
        info->status = htmlToPlainText(statusrxmatch.captured(1));
    if (yearrxmatch.hasMatch())
        info->releaseyear = htmlToPlainText(yearrxmatch.captured(1));
    if (genresrxmatch.hasMatch())
        info->genres = htmlToPlainText(genresrxmatch.captured(1)).trimmed();
    if (summaryrxmatch.hasMatch())
        info->summary = htmlToPlainText(summaryrxmatch.captured(1));
    if (coverrxmatch.hasMatch())
        info->coverlink = coverrxmatch.captured(1);

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
        info->chapertitlesreversed.insert(0, ctitle);
        info->numchapters++;
    }
}

QStringList MangaPanda::getPageList(const QString &chapterlink)
{
    auto job = downloadmanager->downloadAsString(chapterlink);
    QStringList pageLinks;

    if (!job->await(3000))
        return pageLinks;

    QRegularExpression pagerx(R"lit(<option value="([^"]*)")lit");

    int spos = job->buffer.indexOf(R"(<select id="pageMenu" name="pageMenu">)");
    int epos = job->buffer.indexOf("</select>", spos);

    for (auto &match : getAllRxMatches(pagerx, job->buffer, spos, epos))
    {
        pageLinks.append(baseurl + match.captured(1));
    }

    return pageLinks;
}

QString MangaPanda::getImageLink(const QString &pagelink)
{
    auto job = downloadmanager->downloadAsString(pagelink);
    QString imageLink;

    QRegularExpression imagerx(R"lit(<img id="img"[^>]*src="([^"]*)")lit");

    if (!job->await(3000))
        return imageLink;

    auto match = imagerx.match(job->buffer);

    if (match.hasMatch())
        imageLink = match.captured(1);

    return imageLink;
}
