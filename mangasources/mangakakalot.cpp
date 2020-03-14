#include "mangakakalot.h"

#include "defines.h"

Mangakakalot::Mangakakalot(QObject *parent, DownloadManager *dm)
    : AbstractMangaSource(parent, dm)
{
    name = "Mangakakalot";
    baseurl = "https://mangakakalot.com/";
    dicturl =
        "https://mangakakalot.com/"
        "manga_list?type=topview&category=all&state=all&page=";

    mangalist.absoluteUrls = true;
}

MangaList Mangakakalot::getMangaList()
{
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

    QRegularExpression rxstart(R"(<div class="main-wrapper">)");
    QRegularExpression rxend(R"(<div class="panel_page_number">)");
    QRegularExpression rx(
        R"lit(<h3>\s*<a(?: rel="nofollow")? href="([^"]*)"\s*title="([^"]*)")lit");

    QRegularExpression rxnummangas("Total: ([0-9,]+)");
    QRegularExpression rxnumpages(R"(Last\(([0-9]+)\))");

    rxnummangas.indexIn(job->buffer, 0);
    rxnumpages.indexIn(job->buffer, 0);

    nummangas = rxnummangas.cap(1).remove(',').toInt();
    int pages = rxnumpages.cap(1).toInt();

    auto jobs = QVector<QSharedPointer<DownloadStringJob>>(pages);
    jobs[0] = job;

    for (int batch = 0, dli = 1, rxi = 0;
         batch < (pages + maxparalleldownloads - 1) / maxparalleldownloads;
         batch++)
    {
        for (; dli < (batch + 1) * maxparalleldownloads && dli < pages; dli++)
        {
            jobs[dli] = downloadmanager->downloadAsString(
                dicturl + QString::number(dli + 1), -1);
            qDebug() << "dl" << dli << "time:" << timer.elapsed();
        }
        for (; rxi < (batch + 1) * maxparalleldownloads && rxi < pages; rxi++)
        {
            if (!jobs[rxi]->await(10000, true))
            {
                emit updateError(jobs[rxi]->errorString);
                return mangas;
            }

            int spos =
                rxstart.indexIn(jobs[rxi]->buffer, 0) + rx.matchedLength() + 1;
            int epos = rxend.indexIn(jobs[rxi]->buffer, spos);

            int matches = 0;
            for (int pos = spos;
                 (pos = rx.indexIn(jobs[rxi]->buffer, pos)) != -1 && pos < epos;
                 pos += rx.matchedLength())
            {
                mangas.links.append(rx.cap(1));
                mangas.titles.append(
                    htmlToPlainText(htmlToPlainText(rx.cap(2))));
                matches++;
            }

            qDebug() << "rx" << rxi << "time:" << timer.elapsed()
                     << "matches:" << matches;
            jobs[rxi].clear();
        }
        emit updateProgress(100 * rxi / pages);
    }

    qDebug() << "nummangas" << mangalist.links.count();

    emit updateProgress(100);

    return mangas;
}

QSharedPointer<MangaInfo> Mangakakalot::getMangaInfo(const QString &mangalink)
{
    auto job = downloadmanager->downloadAsString(mangalink);

    auto info = QSharedPointer<MangaInfo>(new MangaInfo(this, this));
    info->mangasource = this;
    info->hostname = name;

    info->link = mangalink;

    QRegularExpression titlerx(R"("story-info-right">[^>]*>([^<]*)<)");
    QRegularExpression authorrx(R"(Author\(s\) :[^>]*>[^>]*>[^>]*>([^<]*)<)");
    QRegularExpression statusrx("info-status[^>]*>[^>]*>[^>]*>[^>]*>([^<]*)<");
    QRegularExpression genresrx(R"(Genres :[^>]*>[^>]*>(.*?)</td>)",
                                QRegularExpression::DotMatchesEverythingOption);

    QRegularExpression summaryrx(
        R"lit(class="panel-story-info-description"[^>]*>[^>]*>[^>]*>([^<]*)<)lit");

    if (!job->await(3000))
        return info;

    if (titlerx.indexIn(job->buffer, 0) != -1)
        info->title = htmlToPlainText(titlerx.cap(1));
    if (statusrx.indexIn(job->buffer, 0) != -1)
        info->status = statusrx.cap(1);
    if (authorrx.indexIn(job->buffer, 0) != -1)
        info->author = htmlToPlainText(authorrx.cap(1));
    if (summaryrx.indexIn(job->buffer, 0) != -1)
        info->summary = htmlToPlainText(summaryrx.cap(1));

    auto genresrxmatch = genresrx.match(job->buffer, 0);
    if (genresrxmatch.hasMatch())
        info->genres = htmlToPlainText(genresrxmatch.captured(1)).trimmed();

    info->artist = "-";
    info->releaseyear = "-";

    QRegularExpression coverrx(
        R"lit(<meta name="twitter:image" content="([^"]*)")lit");

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

    QRegularExpression rx(
        R"lit("chapter-name[^>]*href="([^"]*)"[^>]*title="([^"]*)")lit");
    rx.setMinimal(true);

    int spos = job->buffer.indexOf("Chapter name");
    int epos = job->buffer.indexOf("Facebook Comments", spos);

    info->numchapters = 0;
    for (int pos = spos;
         (pos = rx.indexIn(job->buffer, pos)) != -1 && pos < epos;
         pos += rx.matchedLength())
    {
        info->chapters.insert(0, MangaChapter(rx.cap(1), this));

        QString ctitle = rx.cap(2);
        info->chapertitlesreversed.append(ctitle);
        info->numchapters++;
    }

    if (coverlink != "" && !coverjob->await(3000))
    {
        //        info->coverpath = "";
    }

    info->serialize();

    return info;
}

void Mangakakalot::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    int spos = job->buffer.indexOf("LATEST CHAPTERS");
    if (spos == -1)
    {
        info->updateCompeted(false);
        return;
    }

    QRegularExpression chrx("(\\d+)\">");

    int oldnumchapters = info->numchapters;
    int numchapters = info->numchapters;

    if (chrx.indexIn(job->buffer, 0) != -1)
        numchapters = chrx.cap(1).toInt();

    if (numchapters == 0 || numchapters == info->numchapters)
    {
        info->updateCompeted(false);
        return;
    }

    QRegularExpression statusrx("Status:</td>[^>]*>([^<]*)");

    if (statusrx.indexIn(job->buffer, 0) != -1)
        info->status = statusrx.cap(1);

    QRegularExpression rx("<a href=\"([^\"]*)\"[^>]*>([^<]*)</a>([^<]*)");

    spos = job->buffer.indexOf("<div id=\"chapterlist\">");
    int epos = job->buffer.indexOf("<div id=\"adfooter\">", spos);

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
        info->updateCompeted(false);
        return;
    }

    info->serialize();

    info->updateCompeted(true);
}

QStringList Mangakakalot::getPageList(const QString &chapterlink)
{
    auto job = downloadmanager->downloadAsString(chapterlink);
    QStringList pageLinks;

    if (!job->await(3000))
        return pageLinks;

    QRegularExpression rx("<option value=\"([^\"]*)\"");

    int spos =
        job->buffer.indexOf("<select id=\"pageMenu\" name=\"pageMenu\">");
    int epos = job->buffer.indexOf("</select>", spos);

    for (int pos = spos;
         (pos = rx.indexIn(job->buffer, pos)) != -1 && pos < epos;
         pos += rx.matchedLength())
    {
        pageLinks.append(baseurl + rx.cap(1));
    }

    return pageLinks;
}

QString Mangakakalot::getImageLink(const QString &pagelink)
{
    auto job = downloadmanager->downloadAsString(pagelink);
    QString imageLink;

    QRegularExpression rx("src=\"([^\"]*)\"");

    if (!job->await(3000))
        return imageLink;

    int spos = job->buffer.indexOf("<img id=\"img\"");
    int epos = job->buffer.indexOf("/>", spos);

    for (int pos = spos;
         (pos = rx.indexIn(job->buffer, pos)) != -1 && pos < epos;
         pos += rx.matchedLength())
    {
        imageLink = rx.cap(1);
    }

    return imageLink;
}
