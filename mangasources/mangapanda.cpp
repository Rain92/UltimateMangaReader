#include "mangapanda.h"
#include "configs.h"

MangaPanda::MangaPanda(QObject *parent, DownloadManager *dm):
    AbstractMangaSource(parent)
{
    AbstractMangaSource::nummangas = 0;
    AbstractMangaSource::name = "MangaPanda";
    AbstractMangaSource::baseurl = "https://www.mangapanda.com";
    AbstractMangaSource::downloadmanager = dm;
}

bool MangaPanda::updateMangaList()
{
    DownloadStringJob *job = AbstractMangaSource::downloadmanager->downloadAsString(
                                 AbstractMangaSource::baseurl + "/alphabetical");

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
        mangalist.titles.append(rx.cap(2));
        nummangas++;
    }

    qDebug() << nummangas;

    emit progress(100);

    delete job;
    return true;
}


MangaInfo *MangaPanda::getMangaInfo(QString mangalink)
{
    DownloadStringJob *job = AbstractMangaSource::downloadmanager->downloadAsString(mangalink);

    MangaInfo *info = new MangaInfo(this, this);
    info->mangasource = this;
    info->hostname = AbstractMangaSource::name;

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
        info->title = titlerx.cap(1);
    if (yearrx.indexIn(job->buffer, 0) != -1)
        info->releaseyear = yearrx.cap(1);
    if (statusrx.indexIn(job->buffer, 0) != -1)
        info->status = statusrx.cap(1);
    if (artistrx.indexIn(job->buffer, 0) != -1)
        info->artist = artistrx.cap(1);
    if (authorrx.indexIn(job->buffer, 0) != -1)
        info->author = authorrx.cap(1);
    if (summaryrx.indexIn(job->buffer, 0) != -1)
        info->summary = summaryrx.cap(1);


    QRegExp coverrx("<div id=\"mangaimg\"><img src=\"([^\"]*)");

    QString coverlink;
    if (coverrx.indexIn(job->buffer, 0) != -1)
        coverlink = coverrx.cap(1);


    coverlink = coverlink.replace("http:", "https:");
    info->coverpath = downloaddircovers + "/" +
                      makePathLegal(AbstractMangaSource::name + "_" + info->title) +
                      "." + coverlink.right(3);



    DownloadFileJob *coverjob = AbstractMangaSource::downloadmanager->downloadAsFile(coverlink, info->coverpath);

    QRegExp rxstart("<div id=\"chapterlist\">");
    QRegExp rxend("<div id=\"adfooter\">");
    QRegExp rx("<a href=\"([^\"]*)\"[^>]*>([^<]*)</a>([^<]*)");

    int spos = rxstart.indexIn(job->buffer, 0) + rx.matchedLength() + 1;
    int epos = rxend.indexIn(job->buffer, spos);

    info->numchapters = 0;
    for (int pos = spos; (pos = rx.indexIn(job->buffer, pos)) != -1
            && pos < epos; pos += rx.matchedLength())
    {
        info->chapters.append(MangaChapter(AbstractMangaSource::baseurl + rx.cap(1), this));

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


    downloadmanager->fileDownloads->remove(coverlink);
    delete job;
    return info;
}

QStringList *MangaPanda::getPageList(QString chapterlink)
{
//    qDebug() << "getting pages";
    DownloadStringJob *job = AbstractMangaSource::downloadmanager->downloadAsString(chapterlink);
    QStringList *pageLinks = new QStringList();

    if (!job->await(3000))
        return pageLinks;


    QRegExp rxstart("<select id=\"pageMenu\" name=\"pageMenu\">");
    QRegExp rxend("</select>");
    QRegExp rx("<option value=\"([^\"]*)\"");

    int spos = rxstart.indexIn(job->buffer, 0) + rx.matchedLength() + 1;
    int epos = rxend.indexIn(job->buffer, spos);

    for (int pos = spos; (pos = rx.indexIn(job->buffer, pos)) != -1
            && pos < epos; pos += rx.matchedLength())
    {
        pageLinks->append(AbstractMangaSource::baseurl + rx.cap(1));
    }
//    qDebug() << chapterlink << "pages: " << pageLinks->count();

    delete job;
    return pageLinks;
}


QString MangaPanda::getImageLink(QString pagelink)
{
    DownloadStringJob *job = AbstractMangaSource::downloadmanager->downloadAsString(pagelink);
    QString imageLink;

    QRegExp rxstart("<img id=\"img\"");
    QRegExp rxend("/>");
    QRegExp rx("src=\"([^\"]*)\"");

    if (!job->await(3000))
        return imageLink;


    int spos = rxstart.indexIn(job->buffer, 0) + rx.matchedLength() + 1;
    int epos = rxend.indexIn(job->buffer, spos);

    for (int pos = spos; (pos = rx.indexIn(job->buffer, pos)) != -1
            && pos < epos; pos += rx.matchedLength())
    {
        imageLink = rx.cap(1);
    }

    delete job;
    return imageLink;
}


