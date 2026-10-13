#include "mangadex.h"

#include "json.h"

using namespace rapidjson;

namespace
{
struct DexChapter
{
    QString id;
    QString number;
    QString volume;
    QString subtitle;
    QString createdAt;
    bool hasSubtitle = false;
    bool isLoaded = false;
    int serverIndex = 0;

    QString title;
    QString key;
    QString part;
    bool isNumbered = false;
    double volumeKey = 0;
    qint64 wholeKey = 0;
    qint64 partKey = 0;
    int partLength = 0;
};

const char contentRatings[] =
    "&contentRating[]=safe&contentRating[]=suggestive&contentRating[]=erotica&contentRating[]=pornographic";

const QRegularExpression uuidrx(
    R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})");
}  // namespace

static QString getStringSafe(const Value &object, const char *member)
{
    if (!object.IsObject())
        return "";

    auto it = object.FindMember(member);
    if (it == object.MemberEnd() || !it->value.IsString())
        return "";

    return QString::fromUtf8(it->value.GetString(), it->value.GetStringLength());
}

// first non-empty entry of a {"lang": "text"} map
static QString getFirstString(const Value &object)
{
    if (!object.IsObject())
        return "";

    for (auto it = object.MemberBegin(); it != object.MemberEnd(); ++it)
        if (it->value.IsString() && it->value.GetStringLength() > 0)
            return QString::fromUtf8(it->value.GetString(), it->value.GetStringLength()).trimmed();

    return "";
}

static QString getLocalizedString(const Value &object, const char *lang = "en")
{
    auto str = getStringSafe(object, lang);
    if (str.trimmed().isEmpty())
        str = getFirstString(object);

    return str;
}

static QString getMangaTitle(const Value &attributes)
{
    if (!attributes.IsObject())
        return "";

    // the main title of most japanese, korean and chinese series is the romanized original title,
    // prefer the english one: title.en, then the english alt title
    const Value *titles = attributes.HasMember("title") ? &attributes["title"] : nullptr;

    if (titles)
    {
        auto title = getStringSafe(*titles, "en");
        if (!title.trimmed().isEmpty())
            return title;
    }

    QString fallback;
    if (attributes.HasMember("altTitles") && attributes["altTitles"].IsArray())
        for (const auto &alt : attributes["altTitles"].GetArray())
        {
            auto title = getStringSafe(alt, "en").trimmed();
            if (!title.isEmpty())
                return title;
            if (fallback.isEmpty())
                fallback = getFirstString(alt);
        }

    if (titles)
    {
        auto title = getFirstString(*titles);
        if (!title.isEmpty())
            return title;
    }

    return fallback;
}

static QString capitalize(QString str)
{
    if (!str.isEmpty())
        str[0] = str[0].toUpper();
    return str;
}

// manga descriptions are markdown, the result is passed to htmlToPlainText
static QString markdownToHtml(QString text)
{
    static const QRegularExpression linksrx(
        R"(^[\s*_]*(?:[\w ]{0,20} )?Links[\s*_]*:?[\s*_]*$)",
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression mdlinkrx(R"(\[([^\]]*)\]\([^)]*\))");
    static const QRegularExpression bbcoderx(
        R"(\[/?(?:b|i|u|s|url|spoiler|quote|img|size|color|center)(?:=[^\]]*)?\])",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rulerx(R"(^\s*(?:-{3,}|_{3,}|\*{3,})\s*$)",
                                           QRegularExpression::MultilineOption);
    static const QRegularExpression emphasisrx(R"(\*\*|__)");
    static const QRegularExpression italicrx(R"(\*([^*\n]+)\*)");
    static const QRegularExpression blanklinesrx(R"(\n[ \t]*(?:\n[ \t]*)+)");

    text.remove('\r');

    // drop the list of official links many descriptions end with
    auto linksmatch = linksrx.match(text);
    if (linksmatch.hasMatch() && !text.left(linksmatch.capturedStart()).trimmed().isEmpty())
        text.truncate(linksmatch.capturedStart());

    text.replace(mdlinkrx, "\\1");
    text.remove(bbcoderx);
    text.remove(rulerx);
    text.remove(emphasisrx);
    text.replace(italicrx, "\\1");
    text.replace(blanklinesrx, "\n\n");

    return text.trimmed().replace('\n', "<br>");
}

// "014.5" -> 14 and "5", "3.0" -> 3; ranges and text are unnumbered
static bool parseChapterNumber(DexChapter &ch)
{
    static const QRegularExpression numberrx(R"(^(\d{1,15})(?:\.(\d{0,15}))?$)");

    auto match = numberrx.match(ch.number.trimmed());
    if (!match.hasMatch())
        return false;

    ch.part = match.captured(2);
    if (ch.part.count('0') == ch.part.length())
        ch.part.clear();

    ch.wholeKey = match.captured(1).toLongLong();

    return true;
}

// the digits after the dot are either a decimal fraction (24.5, 24.75, 24.9) or a part number
// (14.1, 14.2, ..., 14.9, 14.10, MangaDex sorts them that way). Decimals are not written with
// trailing zeros, so a part 10, 20 or 100 means parts, as does 11 or 12 when part 10 is missing.
// The digits are the identity: 42.8 and 42.08 are different chapters.
static void setPartKeys(QVector<DexChapter> &chapters)
{
    QSet<qint64> numberedParts;
    for (const auto &ch : std::as_const(chapters))
        if (ch.isNumbered && (ch.part.endsWith('0') || ch.part == "11" || ch.part == "12"))
            numberedParts.insert(ch.wholeKey);

    for (auto &ch : chapters)
    {
        if (!ch.isNumbered)
            continue;

        bool isPart = numberedParts.contains(ch.wholeKey);
        ch.key = QString::number(ch.wholeKey) + (ch.part.isEmpty() ? "" : "." + ch.part);
        ch.partLength = ch.part.length();
        ch.partKey = isPart ? ch.part.toLongLong() : (ch.part + QString(15, '0')).left(15).toLongLong();
    }
}

// some series (e.g. anthologies) restart the numbering every volume without being flagged.
// A side story numbered from 1 inside one volume of a normal series must not count,
// so chapter 1 has to be in at least half of the volumes and most numbers in several volumes.
static bool detectVolumeReset(const QVector<DexChapter> &chapters)
{
    QSet<double> volumes;
    QHash<QString, QSet<double>> volumesByNumber;

    for (const auto &ch : chapters)
    {
        bool isVolume = false;
        double volume = ch.volume.toDouble(&isVolume);
        if (!ch.isNumbered || !isVolume)
            continue;

        volumes.insert(volume);
        volumesByNumber[ch.key].insert(volume);
    }

    int numberedChapters = 0;
    int sharedChapters = 0;
    for (const auto &numberVolumes : std::as_const(volumesByNumber))
    {
        numberedChapters += numberVolumes.size();
        if (numberVolumes.size() > 1)
            sharedChapters += numberVolumes.size();
    }

    int volumesWithFirstChapter = volumesByNumber.value("1").size();

    return volumesWithFirstChapter >= 2 && volumesWithFirstChapter * 2 >= volumes.size() &&
           sharedChapters * 2 >= numberedChapters;
}

// chapters in the order of the api (volume, then chapter) -> oldest first, one upload per chapter
static MangaChapterCollection buildChapterList(QVector<DexChapter> &chapters, bool resetOnNewVolume)
{
    for (auto &ch : chapters)
        ch.isNumbered = parseChapterNumber(ch);
    setPartKeys(chapters);

    if (!resetOnNewVolume)
        resetOnNewVolume = detectVolumeReset(chapters);

    int previous = -1;
    for (int i = 0; i < chapters.size(); i++)
    {
        auto &ch = chapters[i];

        bool isVolume = false;
        double volume = ch.volume.toDouble(&isVolume);

        if (!ch.number.trimmed().isEmpty())
        {
            ch.title = "Ch. " + ch.number;
            if (resetOnNewVolume && !ch.volume.isEmpty())
                ch.title = "Vol. " + ch.volume + " " + ch.title;
            if (ch.hasSubtitle)
                ch.title += " " + ch.subtitle;
        }
        else if (!ch.subtitle.trimmed().isEmpty())
        {
            ch.title = ch.subtitle.trimmed();
        }
        else
        {
            // untitled uploads without a number are oneshots or whole volumes
            ch.title = ch.volume.isEmpty() ? QString("Oneshot") : "Vol. " + ch.volume;
        }

        if (ch.isNumbered)
        {
            ch.volumeKey = resetOnNewVolume ? (isVolume ? volume : 1e9) : 0;
            if (resetOnNewVolume)
                ch.key = (isVolume ? QString::number(volume, 'g', 12) : ch.volume) + "|" + ch.key;
            previous = i;
        }
        else
        {
            // unnumbered chapters keep their place behind the preceding chapter
            ch.volumeKey = previous >= 0 ? chapters[previous].volumeKey : -1e9;
            ch.wholeKey = previous >= 0 ? chapters[previous].wholeKey : -1;
            ch.partKey = previous >= 0 ? chapters[previous].partKey : 0;
            ch.partLength = previous >= 0 ? chapters[previous].partLength : 0;
            ch.key = "x|" + (previous >= 0 ? chapters[previous].key : QString()) + "|" + ch.title.toLower();
        }
    }

    // several groups often upload the same chapter, keep one upload of every chapter:
    // an already loaded one (keeps its pages and the reading position), else the first upload
    QHash<QString, int> selectedUploads;
    for (int i = 0; i < chapters.size(); i++)
    {
        const auto &ch = chapters[i];
        auto it = selectedUploads.find(ch.key);

        if (it == selectedUploads.end())
            selectedUploads.insert(ch.key, i);
        else if ((ch.isLoaded != chapters[*it].isLoaded) ? ch.isLoaded
                                                         : ch.createdAt < chapters[*it].createdAt)
            *it = i;
    }

    QVector<DexChapter> selected;
    for (auto i : std::as_const(selectedUploads))
        selected.append(chapters[i]);

    std::sort(selected.begin(), selected.end(),
              [](const DexChapter &a, const DexChapter &b)
              {
                  return std::make_tuple(a.volumeKey, a.wholeKey, a.partKey, a.partLength, !a.isNumbered,
                                         a.serverIndex) < std::make_tuple(b.volumeKey, b.wholeKey, b.partKey,
                                                                          b.partLength, !b.isNumbered,
                                                                          b.serverIndex);
              });

    MangaChapterCollection newchapters;
    for (const auto &ch : std::as_const(selected))
    {
        newchapters.append(MangaChapter(ch.title, ch.id));
    }

    return newchapters;
}

MangaDex::MangaDex(NetworkManager *networkManager)
    : MangaSourceBase(networkManager), apiRequestIntervalMs(0), apiRequestCount(0), atHomeSchedule(0)
{
    name = "MangaDex";
    apiUrl = "https://api.mangadex.org";
    uploadsUrl = "https://uploads.mangadex.org";
    baseUrl = apiUrl;
    atHomeClock.start();

    // the api requires a non-spoofed user agent, so don't send the default browser one
    headerRules.add("mangadex.org", "User-Agent",
                                              "UltimateMangaReader (github.com/Rain92/UltimateMangaReader)");
}

Task<Result<>> MangaDex::downloadThrottled(QStringList urls, UpdateProgressToken *token,
                                           std::function<bool(int, QSharedPointer<DownloadBufferJob>)> handler)
{
    // the api allows about 5 requests per second. Its edge servers reject a random part of the
    // list requests at once with "400 Bad Request", retrying right away almost always works.
    // Only slow down when a request fails again, e.g. when the edge rejects most requests for a while.
    // The edge throttles sustained crawls that way: after a minute or so 30-40% get rejected and about one
    // page per second gets through, no matter how many requests run at once (measured 2026-10). More only
    // adds rejected requests.
    const int maxParallel = 2;
    const int minRequestIntervalMs = 350;
    const int maxRequestIntervalMs = 4000;
    const int timeoutMs = 20000;
    const int maxRetries = 12;
    // all waits are split into short steps, so cancelling is noticed right away
    const int stepMs = 250;

    QElapsedTimer clock;
    clock.start();

    QList<int> pending;
    for (int i = 0; i < urls.size(); i++)
        pending.append(i);

    QVector<int> retries(urls.size(), 0);
    QVector<qint64> retryAt(urls.size(), 0);
    QList<QPair<int, QSharedPointer<DownloadBufferJob>>> running;

    auto isDone = [](const QSharedPointer<DownloadBufferJob> &job)
    { return job->isFinished(); };

    while (!pending.isEmpty() || !running.isEmpty())
    {
        if (token->canceled)
            co_return Err(Error::Cancelled, "Download cancelled");

        // the finished ones, in whatever order they finished
        auto done = std::find_if(running.begin(), running.end(), [&](const auto &r) { return isDone(r.second); });
        if (done != running.end())
        {
            auto [i, job] = *done;
            running.erase(done);

            if (job->isCompleted && handler(i, job))
            {
                apiRequestIntervalMs = apiRequestIntervalMs * 2 / 3;
                continue;
            }

            auto error = job->hasFailed() ? job->error : Error{Error::Parse, "Couldn't parse manga list.", 0};
            if (retries[i]++ >= maxRetries)
                co_return Err(error);

            qDebug() << "MangaDex: retrying" << urls[i] << error.message;
            if (retries[i] > 1)
            {
                apiRequestIntervalMs = apiRequestIntervalMs * 3 / 2;
                retryAt[i] = clock.elapsed() + qMin(1000 << (retries[i] - 1), 15000);
            }
            pending.prepend(i);
            continue;
        }

        auto next = std::find_if(pending.begin(), pending.end(),
                                 [&](int i) { return retryAt[i] <= clock.elapsed(); });
        if (running.size() < maxParallel && next != pending.end())
        {
            apiRequestIntervalMs = qBound(minRequestIntervalMs, apiRequestIntervalMs, maxRequestIntervalMs);
            if (apiRequestTimer.isValid() && apiRequestTimer.elapsed() < apiRequestIntervalMs)
            {
                co_await delay(qMin<qint64>(apiRequestIntervalMs - apiRequestTimer.elapsed(), stepMs));
                continue;
            }

            int i = *next;
            pending.erase(next);
            apiRequestTimer.start();
            apiRequestCount++;

            // the retries are scheduled here
            running.append({i, download(urls[i], timeoutMs, 0)});
            continue;
        }

        if (running.isEmpty())
        {
            // only failed requests waiting for their retry are left
            qint64 nextRetry = retryAt[pending.first()];
            for (int i : std::as_const(pending))
                nextRetry = qMin(nextRetry, retryAt[i]);
            co_await delay(qMin<qint64>(nextRetry - clock.elapsed(), stepMs));
            continue;
        }

        // until any of the running ones finished (they are kept in running meanwhile)
        QList<DownloadJobBase *> jobs;
        for (const auto &r : std::as_const(running))
            jobs.append(r.second.get());
        co_await anyFinished(jobs, stepMs);
    }

    co_return {};
}

Task<Result<>> MangaDex::crawlMangaWindow(QString query, int maxResults, MangaDexUpdateState *statePointer,
                                          UpdateProgressToken *token)
{
    auto &state = *statePointer;
    const int limit = 100;
    // offset + limit must not exceed 10000
    const int numPopular = 2000;

    QList<int> requestedPages;

    auto handler = [&](int i, QSharedPointer<DownloadBufferJob> job) -> bool
    {
        int page = requestedPages[i];
        try
        {
            Document doc;
            ParseResult res = doc.Parse(job->buffer.data());

            if (!res || !doc.IsObject() || getStringSafe(doc, "result") != "ok" || !doc.HasMember("data") ||
                !doc["data"].IsArray())
                return false;

            if (page == 0 && doc.HasMember("total") && doc["total"].IsInt())
                state.total = doc["total"].GetInt();

            QList<QPair<QString, QString>> entries;
            for (const auto &r : doc["data"].GetArray())
            {
                auto id = getStringSafe(r, "id");
                if (id.isEmpty() || !r.HasMember("attributes"))
                    continue;

                const auto &attributes = r["attributes"];
                entries.append({id, getMangaTitle(attributes)});

                auto createdAt = getStringSafe(attributes, "createdAt");
                if (createdAt > state.lastCreatedAt)
                    state.lastCreatedAt = createdAt;
            }

            state.pages.insert(page, entries);

            // the first page of the popular titles has the size of the whole catalogue
            if (state.catalogueSize == 0)
                state.catalogueSize = state.total;
            state.processed += entries.size();
            int expected = state.catalogueSize + qMin(state.catalogueSize, numPopular);
            if (expected > 0)
                token->sendProgress(qBound(0, 100 * state.processed / expected, 99));
        }
        catch (const JsonError &)
        {
            return false;
        }
        return true;
    };

    // the pages a failed attempt got are kept
    if (!state.pages.contains(0))
    {
        requestedPages = {0};
        auto first = co_await downloadThrottled({query.arg(0)}, token, handler);
        if (!first)
            co_return first;
    }

    int numPages = (qMin(state.total, maxResults) + limit - 1) / limit;

    requestedPages.clear();
    QStringList urls;
    for (int p = 1; p < numPages; p++)
        if (!state.pages.contains(p))
        {
            requestedPages.append(p);
            urls.append(query.arg(p * limit));
        }

    auto rest = co_await downloadThrottled(urls, token, handler);
    if (!rest)
        co_return rest;

    for (const auto &entries : std::as_const(state.pages))
        for (const auto &[id, title] : entries)
            if (!state.mangaIds.contains(id))
            {
                state.mangaIds.insert(id);
                state.mangas.append(title, "/manga/" + id);
            }
    state.pages.clear();

    co_return {};
}

Task<MangaListUpdate> MangaDex::fetchMangaList(UpdateProgressToken *token, MangaList)
{
    // a retry continues with the window and the pages the failed attempt didn't get
    auto &state = token->resumeState<MangaDexUpdateState>();
    state.mangas.absoluteUrls = false;

    token->sendProgress(token->sourcesProgress.value(name));

    QElapsedTimer timer;
    timer.start();
    apiRequestCount = 0;
    apiRequestIntervalMs = 0;

    // offset + limit must not exceed 10000
    const int windowSize = 10000;
    const int numPopular = 2000;

    // chapters are english only, so only list titles that have english chapters
    auto query = apiUrl +
                 "/manga?limit=100&offset=%1&availableTranslatedLanguage[]=en&hasAvailableChapters=true" +
                 contentRatings;

    // most followed titles first, so ordering by popularity is meaningful at the top of the list
    if (state.window < 0)
    {
        auto crawled = co_await crawlMangaWindow(query + "&order[followedCount]=desc", numPopular, &state, token);
        if (!crawled)
            co_return MangaListUpdate::failed(crawled.error());

        state.window = 0;
        state.total = 0;
        state.lastCreatedAt.clear();
    }

    // then the whole catalogue, in windows by creation date
    for (; state.window < 100; state.window++)
    {
        auto windowQuery = query + "&order[createdAt]=asc";
        if (!state.since.isEmpty())
            windowQuery += "&createdAtSince=" + state.since;

        auto crawled = co_await crawlMangaWindow(windowQuery, windowSize, &state, token);
        if (!crawled)
            co_return MangaListUpdate::failed(crawled.error());

        if (state.total <= windowSize)
            break;

        // createdAtSince is inclusive, the overlapping entries are skipped as duplicates
        auto next = state.lastCreatedAt.left(19);
        if (next.isEmpty() || next <= state.since)
            co_return MangaListUpdate::failed({Error::Parse, "Couldn't page through the manga list.", 0});
        state.since = next;
        state.total = 0;
        state.lastCreatedAt.clear();
    }

    qDebug() << "mangas:" << state.mangas.size << "requests:" << apiRequestCount << "time:" << timer.elapsed();

    token->sendProgress(100);

    co_return MangaListUpdate::complete(state.mangas);
}

Task<Result<MangaDetails>> MangaDex::fetchMangaInfo(QString mangaUrl, MangaChapterCollection known)
{
    // the manga list has the urls of the mangas, the info also needs its authors and the cover
    auto requestUrl = mangaUrl;
    auto match = uuidrx.match(mangaUrl);
    if (match.hasMatch())
        requestUrl = apiUrl + "/manga/" + match.captured() +
                     "?includes[]=author&includes[]=artist&includes[]=cover_art";

    // the chapter list is loaded right after with its own timeouts, allow for slow wifi here as well
    auto job = download(requestUrl, 6000);
    if (!co_await job)
        co_return Err(job->error);

    MangaDetails details;
    QString mangaId;
    bool resetOnNewVolume = false;

    try
    {
        Document doc;
        ParseResult res = doc.Parse(job->buffer.data());
        if (!res || !doc.IsObject() || getStringSafe(doc, "result") != "ok" || !doc.HasMember("data") ||
            !doc["data"].IsObject())
            co_return Err(Error::Parse, "Couldn't parse manga infos.");

        const auto &data = doc["data"];
        mangaId = getStringSafe(data, "id");

        if (mangaId.isEmpty() || !data.HasMember("attributes") || !data["attributes"].IsObject())
            co_return Err(Error::Parse, "Couldn't parse manga infos.");

        const auto &attributes = data["attributes"];

        details.status = capitalize(getStringSafe(attributes, "status"));

        if (attributes.HasMember("year") && attributes["year"].IsInt())
            details.releaseYear = QString::number(attributes["year"].GetInt());

        QStringList genres;
        QStringList themes;
        auto demographic = getStringSafe(attributes, "publicationDemographic");
        if (!demographic.isEmpty())
            genres.append(capitalize(demographic));

        if (attributes.HasMember("tags") && attributes["tags"].IsArray())
            for (const auto &tag : attributes["tags"].GetArray())
            {
                if (!tag.IsObject() || !tag.HasMember("attributes") || !tag["attributes"].IsObject())
                    continue;

                const auto &tagAttributes = tag["attributes"];
                auto group = getStringSafe(tagAttributes, "group");
                auto tagName =
                    tagAttributes.HasMember("name") ? getLocalizedString(tagAttributes["name"]) : "";

                if (tagName.isEmpty())
                    continue;
                if (group == "genre")
                    genres.append(tagName);
                else if (group == "theme")
                    themes.append(tagName);
            }
        genres.append(themes);
        details.genres = genres.join(", ");

        if (attributes.HasMember("description"))
            details.summary = htmlToPlainText(markdownToHtml(getStringSafe(attributes["description"], "en")));

        resetOnNewVolume = attributes.HasMember("chapterNumbersResetOnNewVolume") &&
                           attributes["chapterNumbersResetOnNewVolume"].IsBool() &&
                           attributes["chapterNumbersResetOnNewVolume"].GetBool();

        QStringList authors;
        QStringList artists;

        if (data.HasMember("relationships") && data["relationships"].IsArray())
            for (const auto &rel : data["relationships"].GetArray())
            {
                if (!rel.IsObject() || !rel.HasMember("attributes"))
                    continue;

                auto type = getStringSafe(rel, "type");
                const auto &relAttributes = rel["attributes"];

                if (type == "author" || type == "artist")
                {
                    auto &names = type == "author" ? authors : artists;
                    auto name = getStringSafe(relAttributes, "name").trimmed();
                    if (!name.isEmpty() && !names.contains(name))
                        names.append(name);
                }
                else if (type == "cover_art")
                {
                    auto fileName = getStringSafe(relAttributes, "fileName");
                    if (!fileName.isEmpty())
                        details.coverUrl =
                            QString("%1/covers/%2/%3.512.jpg").arg(uploadsUrl, mangaId, fileName);
                }
            }

        details.author = authors.join(", ");
        details.artist = artists.join(", ");
    }
    catch (const JsonError &)
    {
        co_return Err(Error::Parse, "Couldn't parse manga infos.");
    }

    QSet<QString> loadedChapters;
    for (const auto &ch : std::as_const(known))
        if (ch.pagesLoaded)
            loadedChapters.insert(ch.chapterUrl);

    auto chapters = co_await fetchChapters(mangaId, resetOnNewVolume, loadedChapters);
    if (!chapters)
        co_return Err(chapters.error());
    details.chapters = *chapters;

    co_return details;
}

Task<Result<MangaChapterCollection>> MangaDex::fetchChapters(QString mangaId, bool resetOnNewVolume,
                                                             QSet<QString> loadedChapters)
{
    // max limit of the feed endpoint, offset + limit must not exceed 10000
    const int limit = 500;
    const int maxResults = 10000;

    auto feedUrl = apiUrl + "/manga/" + mangaId + "/feed?limit=500&offset=%1&translatedLanguage[]=en" +
                   contentRatings +
                   "&includeExternalUrl=0&includeEmptyPages=0&includeFuturePublishAt=0&includeUnavailable=0"
                   "&order[volume]=asc&order[chapter]=asc&order[createdAt]=asc";

    QVector<DexChapter> chapters;
    QSet<QString> chapterIds;
    int total = 1;

    for (int offset = 0; offset < total && offset + limit <= maxResults; offset += limit)
    {
        // the edge rejects requests with 400 while it throttles (e.g. during a list update), a retry usually works
        DownloadRequest request;
        request.url = feedUrl.arg(offset);
        request.timeout = 24000;
        request.retryDelay = 1000;
        request.retryStatuses = {400};
        auto job = download(request);

        // never return a partial list, merging it would delete the pages of the missing chapters
        if (!co_await job)
        {
            auto error = job->error;
            error.message = "Couldn't load chapter list: " + error.message;
            co_return Err(error);
        }

        try
        {
            Document doc;
            ParseResult res = doc.Parse(job->buffer.data());

            if (!res || !doc.IsObject() || getStringSafe(doc, "result") != "ok" || !doc.HasMember("data") ||
                !doc["data"].IsArray())
                co_return Err(Error::Parse, "Couldn't parse chapter list.");

            total = doc.HasMember("total") && doc["total"].IsInt() ? doc["total"].GetInt() : 0;

            const auto &results = doc["data"].GetArray();
            for (const auto &r : results)
            {
                auto id = getStringSafe(r, "id");
                if (id.isEmpty() || chapterIds.contains(id) || !r.HasMember("attributes") ||
                    !r["attributes"].IsObject())
                    continue;
                chapterIds.insert(id);

                const auto &attributes = r["attributes"];

                DexChapter ch;
                ch.id = id;
                ch.number = getStringSafe(attributes, "chapter");
                ch.volume = getStringSafe(attributes, "volume").trimmed();
                ch.subtitle = getStringSafe(attributes, "title");
                ch.hasSubtitle = attributes.HasMember("title") && attributes["title"].IsString();
                ch.createdAt = getStringSafe(attributes, "createdAt");
                ch.isLoaded = loadedChapters.contains(id);
                ch.serverIndex = chapters.size();
                chapters.append(ch);
            }

            if (results.Size() == 0)
                break;
        }
        catch (const JsonError &)
        {
            co_return Err(Error::Parse, "Couldn't parse chapter list.");
        }
    }

    co_return buildChapterList(chapters, resetOnNewVolume);
}

Task<Result<QList<ImageLink>>> MangaDex::fetchPageList(QString chapterUrl)
{
    // the at-home endpoint allows 40 requests per minute: allow 8 at once, then one every 2 s
    // (38 per minute at most), so even during a bulk download a chapter is never delayed for long
    const qint64 atHomeIntervalMs = 2000;
    const qint64 atHomeBurst = 8;

    auto match = uuidrx.match(chapterUrl);
    auto chapterId = match.hasMatch() ? match.captured() : chapterUrl;

    QSharedPointer<DownloadBufferJob> job;
    for (int attempt = 0; attempt < 2; attempt++)
    {
        if (attempt > 0)
            co_await delay(1000);

        auto now = atHomeClock.elapsed();
        atHomeSchedule = qMax(atHomeSchedule, now) + atHomeIntervalMs;
        auto wait = atHomeSchedule - atHomeBurst * atHomeIntervalMs - now;
        if (wait > 0)
            co_await delay(wait);

        job = download(apiUrl + "/at-home/server/" + chapterId, 8000, 0);

        // only retry quick rejections (edge servers, gateways), not timeouts or missing chapters
        if (co_await job || (job->httpStatus != 400 && job->httpStatus < 500))
            break;
    }

    if (!job->isCompleted)
    {
        auto error = job->error;
        if (job->httpStatus == 404)
            error = {Error::NotFound, "Chapter is not available on MangaDex (removed or licensed).", 404};
        if (job->httpStatus == 429)
            error.message = "Too many chapters requested from MangaDex, try again in a minute.";
        co_return Err(error);
    }

    QStringList imageUrls;

    try
    {
        Document doc;
        ParseResult res = doc.Parse(job->buffer.data());

        if (!res || !doc.IsObject() || getStringSafe(doc, "result") != "ok" || !doc.HasMember("chapter") ||
            !doc["chapter"].IsObject())
            co_return Err(Error::Parse, "Couldn't parse page list.");

        const auto &chapter = doc["chapter"];
        auto hash = getStringSafe(chapter, "hash");

        // data-saver pages are jpgs of at most 1080px width: plenty for e-ink screens,
        // a fraction of the size of the originals and much faster to decode on the device.
        // They are loaded from the MangaDex origin server, which the api returns as baseUrl as well:
        // page urls are stored and loaded much later, a MangaDex@Home baseUrl is only valid for about
        // 15 minutes and every image loaded from it would have to be reported to api.mangadex.network,
        // but only the core sees the result of an image download.
        QString quality = "data-saver";
        const char *pagesMember = "dataSaver";
        if (!chapter.HasMember(pagesMember) || !chapter[pagesMember].IsArray() ||
            chapter[pagesMember].Empty())
        {
            quality = "data";
            pagesMember = "data";
        }

        if (hash.isEmpty() || !chapter.HasMember(pagesMember) || !chapter[pagesMember].IsArray())
            co_return Err(Error::Parse, "Couldn't parse page list.");

        for (const auto &page : chapter[pagesMember].GetArray())
            if (page.IsString())
                imageUrls.append(QString("%1/%2/%3/%4")
                                     .arg(uploadsUrl, quality, hash, QString::fromUtf8(page.GetString())));
    }
    catch (const JsonError &)
    {
        co_return Err(Error::Parse, "Couldn't parse page list.");
    }

    co_return ImageLink::fromUrls(imageUrls);
}

