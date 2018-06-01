#include "mangapanda.h"
#include "configs.h"

MangaPanda::MangaPanda(QObject *parent, DownloadManager *dm):
    AbstractMangaSource(parent)
{
    nummangas = 0;
    name = "MangaPanda";
    baseurl = "https://www.mangapanda.com";
    downloadmanager = dm;
}

bool MangaPanda::updateMangaList()
{
    DownloadStringJob *job = downloadmanager->downloadAsString(
                                 baseurl + "/alphabetical");

    if (!job->await(8000))
        return false;

    emit progress(30);

    mangalist.links.clear();
    mangalist.titles.clear();

    QRegExp rxstart("<div class=\"series_col\">");
    QRegExp rxend("<div id=\"adfooter\">");
    QRegExp rx("<li><a href=\"([^\"]*)\"[^>]*>([^<]*)");

    int spos = rxstart.indexIn(job->buffer, 0) + rx.matchedLength() + 1;
    int epos = rxend.indexIn(job->buffer, spos);

    nummangas = 0;
    for (int pos = spos; (pos = rx.indexIn(job->buffer, pos)) != -1 && pos < epos; pos += rx.matchedLength())
    {
        mangalist.links.append(rx.cap(1));
        mangalist.titles.append(htmlToPlainText(rx.cap(2)));
        nummangas++;
    }

    qDebug() << nummangas;

    emit progress(100);

    delete job;
    return true;
}

MangaInfo *MangaPanda::getMangaInfo(QString mangalink)
{
//    qDebug() << mangalink;

    DownloadStringJob *job = downloadmanager->downloadAsString(mangalink);

    MangaInfo *info = new MangaInfo(this, this);
    info->mangasource = this;
    info->hostname = name;

    info->link = mangalink;

    QRegExp titlerx("<h2 class=\"aname\">([^<]*)");
    QRegExp authorrx("Author:</td>[^>]*>([^<]*)");
    QRegExp artistrx("Artist:</td>[^>]*>([^<]*)");
    QRegExp statusrx("Status:</td>[^>]*>([^<]*)");
    QRegExp yearrx("Year of Release:</td>[^>]*>([^<]*)");

    QRegExp summaryrx("<div id=\"readmangasum\">.*<p>([^<]*)</p>");
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


    QRegExp coverrx("<div id=\"mangaimg\"><img src=\"([^\"]*)");

    QString coverlink;
    if (coverrx.indexIn(job->buffer, 0) != -1)
        coverlink = coverrx.cap(1);


    int ind = coverlink.indexOf('?');
    if (ind == -1)
        ind = coverlink.length();
    QString filetype = coverlink.mid(ind - 4, 4);
    coverlink = coverlink.replace("http:", "https:");
    info->coverpath = mangainfodir(name, info->title) + "cover" + filetype;


    DownloadFileJob *coverjob = downloadmanager->downloadAsFile(coverlink, info->coverpath);


    QRegExp rx("<a href=\"([^\"]*)\"[^>]*>([^<]*)</a>([^<]*)");

    int spos = job->buffer.indexOf("<div id=\"chapterlist\">");
    int epos = job->buffer.indexOf("<div id=\"adfooter\">", spos);

    info->numchapters = 0;
    for (int pos = spos; (pos = rx.indexIn(job->buffer, pos)) != -1
            && pos < epos; pos += rx.matchedLength())
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

        info->coverpath = "";
    }

//    info->chapters.removeLast();
//    info->chapertitlesreversed.removeAt(0);
//    info->numchapters--;
//    info->chapters.removeLast();
//    info->chapertitlesreversed.removeAt(0);
//    info->numchapters--;
//    info->chapters.removeLast();
//    info->chapertitlesreversed.removeAt(0);
//    info->numchapters--;


//    downloadmanager->fileDownloads->remove(coverlink);
    delete job;

    info->serialize();

    return info;
}



void MangaPanda::updateMangaInfoFinishedLoading(DownloadStringJob *job, MangaInfo *info)
{
    int spos = job->buffer.indexOf("LATEST CHAPTERS");
    if (spos == -1)
        return;

    QRegExp chrx("(\\d+)\">");

    int oldnumchapters = info->numchapters;
    int numchapters = info->numchapters;

    if (chrx.indexIn(job->buffer, 0) != -1)
        numchapters = chrx.cap(1).toInt();

    if (numchapters == 0 || numchapters == info->numchapters)
        return;


    QRegExp statusrx("Status:</td>[^>]*>([^<]*)");

    if (statusrx.indexIn(job->buffer, 0) != -1)
        info->status = statusrx.cap(1);


    QRegExp rx("<a href=\"([^\"]*)\"[^>]*>([^<]*)</a>([^<]*)");

    spos = job->buffer.indexOf("<div id=\"chapterlist\">");
    int epos = job->buffer.indexOf("<div id=\"adfooter\">", spos);



    for (int pos = spos, c = 0; (pos = rx.indexIn(job->buffer, pos)) != -1
            && pos < epos; pos += rx.matchedLength(), c++)
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
        return;


    info->serialize();

    info->sendUpdated();
}


QStringList MangaPanda::getPageList(const QString &chapterlink)
{
    DownloadStringJob *job = downloadmanager->downloadAsString(chapterlink);
    QStringList pageLinks;

    if (!job->await(3000))
        return pageLinks;


    QRegExp rx("<option value=\"([^\"]*)\"");

    int spos = job->buffer.indexOf("<select id=\"pageMenu\" name=\"pageMenu\">");
    int epos = job->buffer.indexOf("</select>", spos);

    for (int pos = spos; (pos = rx.indexIn(job->buffer, pos)) != -1
            && pos < epos; pos += rx.matchedLength())
    {
        pageLinks.append(baseurl + rx.cap(1));
    }

    delete job;
    return pageLinks;
}


QString MangaPanda::getImageLink(const QString &pagelink)
{
    DownloadStringJob *job = downloadmanager->downloadAsString(pagelink);
    QString imageLink;


    QRegExp rx("src=\"([^\"]*)\"");

    if (!job->await(3000))
        return imageLink;


    int spos = job->buffer.indexOf("<img id=\"img\"");
    int epos = job->buffer.indexOf("/>", spos);

    for (int pos = spos; (pos = rx.indexIn(job->buffer, pos)) != -1
            && pos < epos; pos += rx.matchedLength())
    {
        imageLink = rx.cap(1);
    }

    delete job;
    return imageLink;
}


