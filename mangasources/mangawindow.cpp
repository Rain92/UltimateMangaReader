#include "mangawindow.h"
#include <QTextDocument>


#include "configs.h"
#include "QTime"

MangaWindow::MangaWindow(QObject *parent, DownloadManager *dm):
    AbstractMangaSource(parent)
{
    AbstractMangaSource::nummangas = 0;
    AbstractMangaSource::name = "MangaWindow";
    AbstractMangaSource::baseurl = "https://mangawindow.net";
    AbstractMangaSource::downloadmanager = dm;
}

bool MangaWindow::updateMangaList()
{
    QTime timer;
    timer.start();


    QString basedictlink = AbstractMangaSource::baseurl + "/browse?chapters=1&langs=english&sort=title&page=";

    DownloadStringJob *job = AbstractMangaSource::downloadmanager->downloadAsString(basedictlink + "1");

    if (!job->await(5000))
    {
        emit updateError(job->errorString);
        return false;
    }

    mangalist.links.clear();
    mangalist.titles.clear();

    qDebug() << "time" << timer.elapsed();

    qDebug() << job->buffer.left(1000);


    int cpos = 0;
    int pages = 1;
    QRegExp chsrx("<a class=\"page-link\" @click=\"onClickPage\\((\\d*)");
    while ((cpos = chsrx.indexIn(job->buffer, cpos)) != -1)
    {
        cpos += chsrx.matchedLength();

        pages = chsrx.cap(1).toInt();
        qDebug() << pages;
    }
//    pages = 1;

    qDebug() << pages;

    QVector<DownloadStringJob *> jobs = QVector<DownloadStringJob *>(pages);
    jobs[0] = job;


    QRegExp rxlink("<a class=\"item-title\" href=\"([^\"]*)\">");
    QRegExp rxtitle("(.+)</a>");
    rxtitle.setMinimal(true);


    QString lastname = "";
    int samenamecounter = 1;
    nummangas = 0;
    for (int batch = 0, dli = 1, rxi = 0; batch < (pages + maxparalleldownloads - 1) / maxparalleldownloads; batch++)
    {
        for (; dli < (batch + 1) * maxparalleldownloads && dli < pages; dli++)
        {
            jobs[dli] = AbstractMangaSource::downloadmanager->downloadAsString(basedictlink + QString::number(dli + 1));
            qDebug() << "dl" << dli << "time:" << timer.elapsed();
        }
//        for (rxi = (batch + 1) * maxparalleldownloads - 1; rxi > (batch) * maxparalleldownloads && rxi > 0; rxi--)
        for (; rxi < (batch + 1) * maxparalleldownloads && rxi < pages; rxi++)
        {
            if (!jobs[rxi]->await(15000, true))
            {
                emit updateError(job->errorString);
                return false;
            }

            for (int pos = 0; (pos = rxlink.indexIn(jobs[rxi]->buffer, pos)) != -1; pos += rxlink.matchedLength())
            {
                rxtitle.indexIn(jobs[rxi]->buffer, pos + rxlink.matchedLength());

                QString cname = htmlToPlainText(rxtitle.cap(1));

                if (cname == lastname)
                {
                    samenamecounter++;
                    cname.append( " [" + QString::number(samenamecounter) + "]");
                }
                else
                {
                    samenamecounter = 1;
                    lastname = cname;
                }

                mangalist.links.append(rxlink.cap(1) + "?" + QString::number(samenamecounter));
                mangalist.titles.append(cname);

                nummangas++;
//                qDebug() << rxlink.cap(1);
//                qDebug() << cname;
            }

            qDebug() << "rx" << rxi << "time:" << timer.elapsed();
            delete jobs[rxi];
//            break;
        }
        emit updateProgress(100 * rxi / pages);
//        break;
    }


    emit updateProgress(100);
    qDebug() << "time" << timer.elapsed();
    return true;
}


MangaInfo *MangaWindow::getMangaInfo(QString mangalink)
{

    if (mangalink.left(5) != "https")
        mangalink = AbstractMangaSource::baseurl + mangalink;

    DownloadStringJob *job = AbstractMangaSource::downloadmanager->downloadAsString(mangalink);

    int samenamecounter = 1;
    int suffixind = mangalink.indexOf('?') + 1;
    if (suffixind != 0)
        samenamecounter = mangalink.mid(suffixind, mangalink.length() - suffixind).toInt();

//    qDebug() << samenamecounter;


    MangaInfo *info = new MangaInfo(this, this);
    info->mangasource = this;
    info->hostname = AbstractMangaSource::name;

    info->link = mangalink;

    QRegExp titlerx("<h3 class=\"item-title\">\\s+<a href[^>]*>([^<]*)");
    QRegExp authorrx("Authors:</b>\\s+<span>(.*)</span>");
    authorrx.setMinimal(true);
    QRegExp statusrx("<b>Status:</b>\\s+<span>([^<]*)");

    QRegExp genresrx("Genres:(.*)</span>");
    genresrx.setMinimal(true);

    QRegExp summaryrx("Summary:</h5>\\s+<pre>(?:<p>)?(.*)</");
    summaryrx.setMinimal(true);


    if (!job->await(3000))
    {
        qDebug() << job->errorString;
        return info;
    }

    int spos = job->buffer.indexOf("<div class=\"container\">");

    if (titlerx.indexIn(job->buffer, spos) != -1)
        info->title = htmlToPlainText(titlerx.cap(1));

    if (samenamecounter > 1)
        info->title.append( " [" + QString::number(samenamecounter) + "]");

    if (authorrx.indexIn(job->buffer, spos) != -1)
        info->author = htmlToPlainText(authorrx.cap(1));
    if (statusrx.indexIn(job->buffer, spos) != -1)
        info->status = statusrx.cap(1);
    if (summaryrx.indexIn(job->buffer, spos) != -1)
        info->summary = htmlToPlainText(summaryrx.cap(1));
    if (genresrx.indexIn(job->buffer, 0) != -1)
        info->genres = htmlToPlainText(genresrx.cap(1)).trimmed();

    info->releaseyear = "";

    QRegExp coverrx("<img class=\"shadow-6\" src=\"([^\"]*)");

    QString coverlink;
    if (coverrx.indexIn(job->buffer, spos) != -1)
        coverlink = "https:" + coverrx.cap(1);

    info->coverlink = coverlink;

    int ind = coverlink.indexOf('?');
    if (ind == -1)
        ind = coverlink.length();
    QString filetype = coverlink.mid(ind - 4, 4);
    info->coverpath = mangainfodir(name, info->title) + "cover" + filetype;

//    qDebug() << info->title;
//    qDebug() << coverlink << filetype;
//    qDebug() << info->coverpath;

    DownloadFileJob *coverjob = AbstractMangaSource::downloadmanager->downloadAsFile(coverlink, info->coverpath);

    QRegExp rx("<a class=\"chapt\" href=\"([^\"]*)\">\\s+<b>([^<]*)");

    spos = job->buffer.indexOf("<!-- chapters -->");
    int epos = job->buffer.indexOf("<!-- /chapters -->", spos);

    info->numchapters = 0;
    for (int pos = spos; (pos = rx.indexIn(job->buffer, pos)) != -1 && pos < epos; pos += rx.matchedLength())
    {

        info->chapters.insert(0, MangaChapter(baseurl + rx.cap(1), this));

        info->chapertitlesreversed.append(rx.cap(2));
        info->numchapters++;
//        qDebug() << info->chapters[0].chapterlink;
//        qDebug() << rx.cap(2);
    }

    if (coverlink != "" && !coverjob->await(6000))
    {

//        qDebug() << "coverload failed";
//        info->coverpath = "";
    }
    delete job;

    info->serialize();

    return info;
}


void MangaWindow::updateMangaInfoFinishedLoading(DownloadStringJob *job, MangaInfo *info)
{
    int spos = job->buffer.indexOf("<!-- chapters -->");
    int epos = job->buffer.indexOf("<!-- /chapters -->", spos);
    if (spos == -1)
        return;

    QString countstr = job->buffer.mid(spos, epos - spos);

    QRegExp chrx("(\\d+)\">");

    int oldnumchapters = info->numchapters;
    int numchapters = countstr.count("<a class=\"chapt\" href=");

    if (numchapters == 0 || numchapters <= info->numchapters)
        return;


    QRegExp statusrx("<b>Status:</b>\\s+<span>([^<]*)");

    if (statusrx.indexIn(job->buffer, 0) != -1)
        info->status = statusrx.cap(1);

    QRegExp rx("<a class=\"chapt\" href=\"([^\"]*)\">\\s+<b>([^<]*)");


    int insi = 0;
    int chapterstoadd = numchapters - oldnumchapters;
    for (int pos = spos; (pos = rx.indexIn(job->buffer, pos)) != -1 && pos < epos && chapterstoadd > 0;
            pos += rx.matchedLength(), chapterstoadd--)
    {

        info->chapters.insert(oldnumchapters, MangaChapter(baseurl + rx.cap(1), this));

        info->chapertitlesreversed.insert(insi++, rx.cap(2));
        info->numchapters++;
//        qDebug() << info->chapters[0].chapterlink;
//        qDebug() << rx.cap(2);
    }

    qDebug() << "update";
    info->serialize();

    info->sendUpdated();
}


QStringList MangaWindow::getPageList(const QString &chapterlink)
{
    DownloadStringJob *job = AbstractMangaSource::downloadmanager->downloadAsString(chapterlink);
    QStringList pageLinks;

    if (!job->await(3000))
        return pageLinks;


    int spos = job->buffer.indexOf("var images = {");
    int epos = job->buffer.indexOf("}", spos);

    QRegExp rx("(https://[^\"]*)");

    if (rx.indexIn(job->buffer, 0) == -1)
        return pageLinks;

    for (int pos = spos; (pos = rx.indexIn(job->buffer, pos)) != -1 && pos < epos; pos += rx.matchedLength())
    {
        pageLinks.append(rx.cap(1));
    }

    delete job;
    return pageLinks;
}


QString MangaWindow::getImageLink(const QString &pagelink)
{
    return pagelink;
}
