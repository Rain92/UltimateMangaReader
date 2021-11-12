#include "mangadex.h"

using namespace rapidjson;

inline QString getStringSafe(const GenericValue<UTF8<>> &jsonobject, const char *member)
{
    if (!jsonobject.HasMember(member))
        return "";

    auto &jm = jsonobject[member];

    if (jm.IsNull())
        return "";

    return jm.GetString();
}

QString padChapterNumber(const QString &number, int places = 4)
{
    auto range = number.split('-');
    QStringList result;
    std::transform(range.begin(), range.end(), std::back_inserter(result),
                   [places](QString chapter)
                   {
                       chapter = chapter.trimmed();
                       auto digits = chapter.split('.')[0].length();
                       return QString("0").repeated(qMax(0, places - digits)) + chapter;
                   });
    return result.join('-');
}

MangaDex::MangaDex(NetworkManager *dm) : AbstractMangaSource(dm)
{
    name = "MangaDex";
    apiUrl = "https://api.mangadex.org";
    baseUrl = apiUrl;
    serverUrls = {"http://s2.mangadex.org/data/", "http://s5.mangadex.org/data/"};

    networkManager->addCookie(".mangadex.org", "mangadex_h_toggle", "1");
    networkManager->addCookie(".mangadex.org", "mangadex_title_mode", "2");
    networkManager->addCookie(".mangadex.org", "mangadex_filter_langs", "1");

    statuses = {"Ongoing", "Completed", "Cancelled", "Hiatus"};
    demographies = {"Shounen", "Shoujo", "Seinen", "Josei"};
    genreMap.insert(2, "Action");
    genreMap.insert(3, "Adventure");
    genreMap.insert(5, "Comedy");
    genreMap.insert(8, "Drama");
    genreMap.insert(9, "Ecchi");
    genreMap.insert(10, "Fantasy");
    genreMap.insert(13, "Historical");
    genreMap.insert(14, "Horror");
    genreMap.insert(17, "Mecha");
    genreMap.insert(18, "Medical");
    genreMap.insert(20, "Mystery");
    genreMap.insert(22, "Psychological");
    genreMap.insert(23, "Romance");
    genreMap.insert(25, "Sci-Fi");
    genreMap.insert(28, "Shoujo Ai");
    genreMap.insert(30, "Shounen Ai");
    genreMap.insert(31, "Slice of Life");
    genreMap.insert(32, "Smut");
    genreMap.insert(33, "Sports");
    genreMap.insert(35, "Tragedy");
    genreMap.insert(37, "Yaoi");
    genreMap.insert(38, "Yuri");
    genreMap.insert(41, "Isekai");
    genreMap.insert(49, "Gore");
    genreMap.insert(50, "Sexual Violence");
    genreMap.insert(51, "Crime");
    genreMap.insert(52, "Magical Girls");
    genreMap.insert(53, "Philosophical");
    genreMap.insert(54, "Superhero");
    genreMap.insert(55, "Thriller");
    genreMap.insert(56, "Wuxia");
}

bool MangaDex::updateMangaList(UpdateProgressToken *token)
{
    MangaList mangas;
    token->sendProgress(10);

    QElapsedTimer timer;
    timer.start();

    auto mangasQuerry = apiUrl +
                        "/manga?limit=100&offset=%1"
                        "&publicationDemographic[]=%2"
                        "&order[createdAt]=%3"
                        "&contentRating[]=safe"
                        "&contentRating[]=suggestive"
                        "&contentRating[]=erotica"
                        "&contentRating[]=pornographic";

    // ugly workaround, current search limit is 10000 entries
    // so we need to search multiple times with different filters
    const char *demographics[] = {"seinen", "none", "none", "josei", "shoujo", "shounen"};
    const char *order[] = {"asc", "asc", "desc", "asc", "asc", "asc"};

    int maxnummangas = 50000;
    int matches = 0;

    QSet<QString> mangaids;

    for (uint i = 0; i < sizeof(demographics) / (sizeof(demographics[0])); i++)
        for (int offset = 0; offset < 10000; offset += 100)
        {
            auto mangasUrl = mangasQuerry.arg(offset).arg(demographics[i], order[i]);
            auto job = networkManager->downloadAsString(mangasUrl, -1);

            if (!job->await(7000))
            {
                token->sendError(job->errorString);
                return false;
            }

            try
            {
                Document doc;
                ParseResult res = doc.Parse(job->buffer.data());

                if (!res)
                    return false;

                if (doc.HasMember("result") && QString(doc["result"].GetString()) == "error")
                    return false;

                auto results = doc["data"].GetArray();

                for (const auto &r : results)
                {
                    auto title = getStringSafe(r["attributes"]["title"], "en");
                    auto id = getStringSafe(r, "id");
                    auto url = QString("/manga/") + id;

                    if (!mangaids.contains(id))
                    {
                        mangas.append(title, url);
                        matches++;
                        mangaids.insert(id);
                    }
                }

                token->sendProgress(10 + 90 * matches / maxnummangas);

                if (results.Size() < 100)
                    break;
            }
            catch (QException &)
            {
                return false;
            }
        }

    this->mangaList = mangas;

    qDebug() << "mangas:" << mangas.size << "time:" << timer.elapsed();

    token->sendProgress(100);

    return true;
}

Result<MangaChapterCollection, QString> MangaDex::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    //    QElapsedTimer t;
    //    t.start();
    QRegularExpression bbrx(R"(\[.*?\])");

    MangaChapterCollection newchapters;

    try
    {
        Document doc;
        ParseResult res = doc.Parse(job->buffer.data());
        if (!res)
            return Err(QString("Coulnd't parse manga infos."));

        auto &mangaObject = doc["data"]["attributes"];

        //        info->author = htmlToPlainText(QString(mangaObject["author"].GetString()));
        //        info->artist = htmlToPlainText(QString(mangaObject["artist"].GetString()));

        info->status = getStringSafe(mangaObject, "status");

        info->releaseYear = getStringSafe(mangaObject, "year");

        info->genres = getStringSafe(mangaObject, "publicationDemographic");

        info->summary = htmlToPlainText(getStringSafe(mangaObject["description"], "en")).remove(bbrx);

        auto rels = doc["data"]["relationships"].GetArray();

        for (const auto &rel : rels)
        {
            auto id = getStringSafe(rel, "id");

            if (getStringSafe(rel, "type") == "cover_art")
            {
                auto jobCover = networkManager->downloadAsString(apiUrl + "/cover/" + id, -1);

                if (jobCover->await(3000))
                {
                    Document coverdoc;
                    ParseResult cres = coverdoc.Parse(jobCover->buffer.data());
                    if (cres)
                    {
                        info->coverUrl = QString("https://uploads.mangadex.org/covers/%1/%2.256.jpg")
                                             .arg(getStringSafe(doc["data"], "id"),
                                                  getStringSafe(coverdoc["data"]["attributes"], "fileName"));
                    }
                }
            }
        }

        int totalchapters = 100;
        QStringList chapternumberlist;

        for (int offset = 0; offset < totalchapters; offset += 100)
        {
            auto params = QString("manga=%1&limit=100&offset=%2&translatedLanguage[]=en")
                              .arg(getStringSafe(doc["data"], "id"))
                              .arg(offset);
            auto jobChapters = networkManager->downloadAsString(apiUrl + "/chapter?" + params, -1);

            if (jobChapters->await(3000))
            {
                Document chaptersdoc;
                ParseResult cres = chaptersdoc.Parse(jobChapters->buffer.data());

                if (getStringSafe(chaptersdoc, "result") == "error")
                    return Err(QString("Couldn't parse chapter list."));

                auto results = chaptersdoc["data"].GetArray();

                totalchapters = chaptersdoc["total"].GetInt();

                for (const auto &r : results)
                {
                    auto chapterId = getStringSafe(r, "id");

                    if (chapterId == "")
                        continue;

                    QString numChapter = getStringSafe(r["attributes"], "chapter");
                    if (numChapter == "")
                        numChapter = "0";

                    QString chapterTitle = "Ch. " + numChapter;

                    if (!r["attributes"]["title"].IsNull())
                        chapterTitle += " " + getStringSafe(r["attributes"], "title");

                    MangaChapter mangaChapter(chapterTitle, chapterId);
                    mangaChapter.chapterNumber = padChapterNumber(numChapter);
                    newchapters.append(mangaChapter);
                    chapternumberlist.append(padChapterNumber(numChapter));
                }
                if (results.Size() < 100)
                    break;
            }
        }
        int size = newchapters.size();

        QVector<int> indices(size);
        QVector<int> indicesInv(size);
        for (int i = 0; i < size; ++i)
            indices[i] = i;

        std::sort(indices.begin(), indices.end(),
                  [&chapternumberlist](int a, int b) {
                      return QString::compare(chapternumberlist[a], chapternumberlist[b],
                                              Qt::CaseInsensitive) < 0;
                  });

        for (int i = 0; i < size; ++i)
            indicesInv[indices[i]] = i;

        for (int i = 0; i < size; i++)
            while (i != indicesInv[i])
            {
                int j = indicesInv[i];

                newchapters.swapItemsAt(i, j);
                indicesInv.swapItemsAt(i, j);
            }
    }
    catch (QException &)
    {
        return Err(QString("Coulnd't parse manga ìnfos."));
    }

    return Ok(newchapters);
}

Result<QStringList, QString> MangaDex::getPageList(const QString &chapterUrl)
{
    auto job = networkManager->downloadAsString(apiUrl + "/chapter/" + chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    QStringList imageUrls;

    try
    {
        Document chapterdoc;
        ParseResult cres = chapterdoc.Parse(job->buffer.data());

        if (getStringSafe(chapterdoc, "result") == "error")
            return Err(QString("Couldn't parse page list."));

        auto hash = getStringSafe(chapterdoc["data"]["attributes"], "hash");

        auto pages = chapterdoc["data"]["attributes"]["data"].GetArray();

        for (const auto &page : pages)
            imageUrls.append(serverUrls.first() + hash + "/" + page.GetString());
    }
    catch (QException &)
    {
        return Err(QString("Coulnd't parse page list."));
    }

    return Ok(imageUrls);
}
