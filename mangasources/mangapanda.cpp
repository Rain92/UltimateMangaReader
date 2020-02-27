#include "mangapanda.h"

#include "configs.h"

MangaPanda::MangaPanda(QObject *parent, DownloadManager *dm)
    : AbstractMangaSource(parent, dm)
{
    name = "MangaPanda";
    baseurl = "http://www.mangapanda.com";
}

bool MangaPanda::updateMangaList()
{
    auto job = downloadmanager->downloadAsString(baseurl + "/alphabetical");

    if (!job->await(8000))
    {
        emit updateError(job->errorString);
        return false;
    }

    emit updateProgress(30);

    mangalist.links.clear();
    mangalist.titles.clear();

    QElapsedTimer timer;
    timer.start();

    int spos = job->buffer.indexOf(R"(<div class="series_col">)");
    int epos = job->buffer.indexOf(R"(<div id="adfooter">)");

    QRegularExpression rx(R"lit(<li><a href="([^"]*)"[^>]*>([^<]*))lit");

    QRegularExpressionMatchIterator rxit = rx.globalMatch(job->buffer, spos);

    nummangas = 0;
    for (auto rxmatch = rxit.next();
         rxit.isValid() && rxmatch.capturedStart() < epos;
         rxmatch = rxit.next())
    {
        mangalist.links.append(rxmatch.captured(1));
        mangalist.titles.append(htmlToPlainText(rxmatch.captured(2)));
        nummangas++;
    }

    auto time = timer.nsecsElapsed() / 1000;

    qDebug() << nummangas << "time:" << time;

    emit updateProgress(100);

    return true;
}

QSharedPointer<MangaInfo> MangaPanda::getMangaInfo(const QString &mangalink)
{
    auto job = downloadmanager->downloadAsString(mangalink);

    auto info = QSharedPointer<MangaInfo>(new MangaInfo(this, this));
    info->mangasource = this;
    info->hostname = name;

    info->link = mangalink;

    QRegExp titlerx(R"(<h2 class="aname">([^<]*))");
    QRegExp authorrx("Author:</td>[^>]*>([^<]*)");
    QRegExp artistrx("Artist:</td>[^>]*>([^<]*)");
    QRegExp statusrx("Status:</td>[^>]*>([^<]*)");
    QRegExp yearrx("Year of Release:</td>[^>]*>([^<]*)");
    QRegExp genresrx("Genre:</td>(.*)</td>");
    genresrx.setMinimal(true);

    QRegExp summaryrx(R"(<div id="readmangasum">.*<p>([^<]*)</p>)");
    summaryrx.setMinimal(true);

    if (!job->await(3000))
        return info;

    if (titlerx.indexIn(job->buffer, 0) != -1)
        info->title = htmlToPlainText(titlerx.cap(1));
    if (yearrx.indexIn(job->buffer, 0) != -1)
        info->releaseyear = yearrx.cap(1);
    if (statusrx.indexIn(job->buffer, 0) != -1)
        info->status = statusrx.cap(1);
    if (artistrx.indexIn(job->buffer, 0) != -1)
        info->artist = htmlToPlainText(artistrx.cap(1));
    if (authorrx.indexIn(job->buffer, 0) != -1)
        info->author = htmlToPlainText(authorrx.cap(1));
    if (summaryrx.indexIn(job->buffer, 0) != -1)
        info->summary = htmlToPlainText(summaryrx.cap(1));
    if (genresrx.indexIn(job->buffer, 0) != -1)
        info->genres = htmlToPlainText(genresrx.cap(1)).trimmed();

    QRegExp coverrx(R"(<div id="mangaimg"><img src="([^"]*))");

    QString coverlink;
    if (coverrx.indexIn(job->buffer, 0) != -1)
        coverlink = coverrx.cap(1);

    info->coverlink = coverlink;

    int ind = coverlink.indexOf('?');
    if (ind == -1)
        ind = coverlink.length();
    QString filetype = coverlink.mid(ind - 4, 4);
    info->coverpath = mangainfodir(name, info->title) + "cover" + filetype;

    auto coverjob = downloadmanager->downloadAsFile(coverlink, info->coverpath);

    QRegExp rx(R"lit(<a href="([^"]*)"[^>]*>([^<]*)</a>([^<]*))lit");

    int spos = job->buffer.indexOf(R"(<div id="chapterlist">)");
    int epos = job->buffer.indexOf(R"(<div id="adfooter">)", spos);

    info->numchapters = 0;
    for (int pos = spos;
         (pos = rx.indexIn(job->buffer, pos)) != -1 && pos < epos;
         pos += rx.matchedLength())
    {
        info->chapters.append(MangaChapter(baseurl + rx.cap(1), this));

        QString ctitle = rx.cap(2);
        if (rx.cap(3) != " : ")
            ctitle += rx.cap(3);
        info->chapertitlesreversed.insert(0, ctitle);
        info->numchapters++;
    }

    if (coverlink != "" && !coverjob->await(3000))
    {
        //        info->coverpath = "";
    }

    info->serialize();

    return info;
}

void MangaPanda::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    int spos = job->buffer.indexOf("LATEST CHAPTERS");
    if (spos == -1)
    {
        info->sendUpdated(false);
        return;
    }

    QRegExp chrx(R"lit((\d+)">)lit");

    int oldnumchapters = info->numchapters;
    int numchapters = info->numchapters;

    if (chrx.indexIn(job->buffer, 0) != -1)
        numchapters = chrx.cap(1).toInt();

    if (numchapters == 0 || numchapters == info->numchapters)
    {
        info->sendUpdated(false);
        return;
    }

    QRegExp statusrx("Status:</td>[^>]*>([^<]*)");

    if (statusrx.indexIn(job->buffer, 0) != -1)
        info->status = statusrx.cap(1);

    QRegExp rx(R"lit(<a href="([^"]*)"[^>]*>([^<]*)</a>([^<]*))lit");

    spos = job->buffer.indexOf(R"(<div id="chapterlist">)");
    int epos = job->buffer.indexOf(R"(<div id="adfooter">)", spos);

    for (int pos = spos, c = 0;
         (pos = rx.indexIn(job->buffer, pos)) != -1 && pos < epos;
         pos += rx.matchedLength(), c++)
    {
        if (c < info->numchapters)
            continue;

        info->chapters.append(MangaChapter(baseurl + rx.cap(1), this));

        QString ctitle = rx.cap(2);
        if (rx.cap(3) != " : ")
            ctitle += rx.cap(3);
        info->chapertitlesreversed.insert(0, ctitle);

        info->numchapters++;
    }

    if (oldnumchapters == info->numchapters)
    {
        info->sendUpdated(false);
        return;
    }

    info->serialize();

    info->sendUpdated(true);
}

QStringList MangaPanda::getPageList(const QString &chapterlink)
{
    auto job = downloadmanager->downloadAsString(chapterlink);
    QStringList pageLinks;

    if (!job->await(3000))
        return pageLinks;

    QRegExp rx(R"lit(<option value="([^"]*)")lit");

    int spos = job->buffer.indexOf(R"(<select id="pageMenu" name="pageMenu">)");
    int epos = job->buffer.indexOf("</select>", spos);

    for (int pos = spos;
         (pos = rx.indexIn(job->buffer, pos)) != -1 && pos < epos;
         pos += rx.matchedLength())
    {
        pageLinks.append(baseurl + rx.cap(1));
    }

    return pageLinks;
}

QString MangaPanda::getImageLink(const QString &pagelink)
{
    auto job = downloadmanager->downloadAsString(pagelink);
    QString imageLink;

    QRegExp rx(R"lit(src="([^"]*)")lit");

    if (!job->await(3000))
        return imageLink;

    int spos = job->buffer.indexOf(R"(<img id="img")");
    int epos = job->buffer.indexOf("/>", spos);

    for (int pos = spos;
         (pos = rx.indexIn(job->buffer, pos)) != -1 && pos < epos;
         pos += rx.matchedLength())
    {
        imageLink = rx.cap(1);
    }

    return imageLink;
}
