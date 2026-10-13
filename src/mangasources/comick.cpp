#include "comick.h"

#include <cmath>

#include "json.h"
#include "utils.h"

using namespace rapidjson;

// backend: comick.art, the unofficial mirror that continues the comick database (same slugs and
// api). the original comick.io (now comick.dev) hosts no chapter images since september 2025.
// nearly all pages and covers are webp.

static const char *userAgent =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36";

// the search api returns 50 titles per request and can only be paged with a cursor
static const int popularPages = 20;
// the sitemaps are only regenerated once a day, the newest titles are missing there
static const int latestPages = 2;
// series that need all uploads (60 per chapter list page) are only handled up to this size
static const int maxListPages = 5;
// pages that are warmed up in the background ahead of the requested one
static const int prefetchPages = 3;

struct ComickCacheWarmUp
{
    QNetworkAccessManager *networkAccessManager = nullptr;
    QNetworkRequest request;
    QElapsedTimer timer;
    int timeout = 0;
    bool finished = false;
    bool cached = false;
    QEventLoop *loop = nullptr;
    std::function<void()> onCached;
};

using CacheWarmUp = ComickCacheWarmUp;

namespace
{
// one chapter upload of the chapter list or the chapter endpoint
struct Upload
{
    QString hid;
    QString chap;
    QString rawChap;
    QString vol;
    QString lang;
    QString title;
    QString group;
    double number = 0;
    double volume = 0;
    bool hasVolume = false;
    // volume of a numbered chapter, from another upload of it if necessary (-1 if unknown)
    double knownVolume = -1;

    // whole volumes are uploaded without a chapter number
    bool isVolume() const { return hasVolume && (chap.isEmpty() || chap == "0"); }
};
}  // namespace

static void cacheWarmUpAttempt(QSharedPointer<CacheWarmUp> warmUp)
{
    auto reply = warmUp->networkAccessManager->head(warmUp->request);
    DownloadJobBase::prepareReply(reply);

    int remaining = warmUp->timeout - int(warmUp->timer.elapsed());
    QTimer::singleShot(qBound(100, remaining, 2000), reply, &QNetworkReply::abort);

    QObject::connect(reply, &QNetworkReply::finished, reply,
                     [warmUp, reply]()
                     {
                         reply->deleteLater();

                         int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                         bool throttled = status == 429 || status >= 500;

                         if (throttled && warmUp->timer.elapsed() + 400 < warmUp->timeout)
                         {
                             // no standby in between, it would stop the timer
                             auto hold = DownloadJobBase::holdActivity();
                             QTimer::singleShot(400, warmUp->networkAccessManager,
                                                [warmUp, hold]() { cacheWarmUpAttempt(warmUp); });
                             return;
                         }

                         if (throttled)
                             qDebug() << "Comick: image still throttled:" << warmUp->request.url();

                         warmUp->finished = true;
                         warmUp->cached = status >= 200 && status < 400;

                         if (warmUp->loop)
                             warmUp->loop->quit();
                         if (warmUp->cached && warmUp->onCached)
                             warmUp->onCached();
                     });
}

// images that are not in the cloudflare cache yet are often answered with 429 for a few seconds
// (the origin is throttled for all users). HEAD requests (retried while throttled) pull the image
// into the cache without transferring it, so the actual download afterwards is served from there.
static QSharedPointer<CacheWarmUp> startCacheWarmUp(QNetworkAccessManager *networkAccessManager,
                                                    const QString &imageUrl, int timeout)
{
    auto warmUp = QSharedPointer<CacheWarmUp>::create();
    warmUp->networkAccessManager = networkAccessManager;
    warmUp->request = QNetworkRequest(QUrl(imageUrl));
    warmUp->request.setRawHeader("User-Agent", userAgent);
    warmUp->request.setRawHeader("Referer", "https://comick.art/");
    warmUp->timeout = timeout;
    warmUp->timer.start();

    cacheWarmUpAttempt(warmUp);

    return warmUp;
}

// waits until the warm-up is done, at most 'maxWait' ms
static void awaitCacheWarmUp(const QSharedPointer<CacheWarmUp> &warmUp, int maxWait)
{
    int remaining = qMin(maxWait, warmUp->timeout - int(warmUp->timer.elapsed()));
    if (warmUp->finished || remaining <= 0)
        return;

    auto hold = DownloadJobBase::holdActivity();
    QEventLoop loop;
    warmUp->loop = &loop;
    QTimer::singleShot(remaining, &loop, &QEventLoop::quit);
    loop.exec();
    warmUp->loop = nullptr;
}

static QString getStringSafe(const GenericValue<UTF8<>> &jsonobject, const char *member)
{
    if (!jsonobject.IsObject() || !jsonobject.HasMember(member))
        return "";

    auto &jm = jsonobject[member];

    if (jm.IsString())
        return QString::fromUtf8(jm.GetString(), jm.GetStringLength());
    if (jm.IsInt64())
        return QString::number(jm.GetInt64());
    if (jm.IsNumber())
        return QString::number(jm.GetDouble());

    return "";
}

static QString joinNames(const GenericValue<UTF8<>> &jsonobject, const char *member)
{
    QStringList names;

    if (jsonobject.HasMember(member) && jsonobject[member].IsArray())
        for (const auto &entry : jsonobject[member].GetArray())
        {
            auto name = getStringSafe(entry, "name").trimmed();
            if (!name.isEmpty() && !names.contains(name))
                names.append(name);
        }

    return names.join(", ");
}

// e.g. "31.2.5" is sorted as 31.2
static double parseNumber(const QString &number)
{
    static const QRegularExpression numberrx(R"lit(^\d+(?:\.\d+)?)lit");

    bool ok = false;
    double value = number.toDouble(&ok);
    if (ok)
        return value;

    auto match = numberrx.match(number);
    return match.hasMatch() ? match.captured().toDouble() : 0;
}

// missing numbers are sometimes sent as the string "None"
static QString getNumberSafe(const GenericValue<UTF8<>> &jsonobject, const char *member)
{
    auto number = getStringSafe(jsonobject, member).trimmed();
    return number.compare("None", Qt::CaseInsensitive) == 0 ? QString() : number;
}

static Upload parseUpload(const GenericValue<UTF8<>> &c)
{
    Upload upload;
    upload.hid = getStringSafe(c, "hid");
    upload.rawChap = getStringSafe(c, "chap").trimmed();
    upload.chap = getNumberSafe(c, "chap");
    upload.vol = getNumberSafe(c, "vol");
    upload.lang = getStringSafe(c, "lang");
    upload.title = getStringSafe(c, "title").trimmed();
    upload.number = parseNumber(upload.chap);
    upload.volume = upload.vol.toDouble(&upload.hasVolume);
    if (upload.number > 0 && upload.hasVolume)
        upload.knownVolume = upload.volume;

    if (c.IsObject() && c.HasMember("group_name") && c["group_name"].IsArray())
        for (const auto &group : c["group_name"].GetArray())
            if (group.IsString())
                upload.group += QString::fromUtf8(group.GetString(), group.GetStringLength()) + '\n';

    return upload;
}

// the english uploads of a json array
static void parseUploads(const GenericValue<UTF8<>> &chapters, QList<Upload> &uploads)
{
    if (!chapters.IsArray())
        return;

    for (const auto &c : chapters.GetArray())
    {
        auto upload = parseUpload(c);
        if (!upload.hid.isEmpty() && (upload.lang.isEmpty() || upload.lang == "en"))
            uploads.append(upload);
    }
}

// "<slug>/<hid>-chapter-<chap>-<lang>", appended to the api url for the page list
static QString chapterPath(const QString &slug, const Upload &upload)
{
    return QString("%1/%2-chapter-%3-%4")
        .arg(slug, upload.hid, upload.rawChap.isEmpty() ? "0" : upload.rawChap,
             upload.lang.isEmpty() ? "en" : upload.lang);
}

// byVolume: the chapter numbers start again in every volume
static MangaChapter makeChapter(const QString &slug, const Upload &upload, bool byVolume = false)
{
    QString name;

    if (upload.hasVolume && (upload.chap.isEmpty() || (upload.chap == "0" && !byVolume)))
    {
        name = "Vol. " + upload.vol;
    }
    else if (byVolume && upload.hasVolume)
    {
        name = "Vol. " + upload.vol + " Ch. " + upload.chap;
    }
    else if (upload.chap.isEmpty())
    {
        name = upload.title.isEmpty() ? QString("Oneshot") : upload.title;
    }
    else
    {
        name = "Ch. " + upload.chap;
    }

    if (!upload.title.isEmpty() && name != upload.title)
        name += ": " + upload.title;

    return MangaChapter(name, chapterPath(slug, upload));
}

// keeps one upload per key: the one of the current chapters, else the first one
template <typename Key>
static void chooseUpload(QMap<Key, Upload> &chosen, const Key &key, const Upload &upload,
                         const QSet<QString> &currentHids)
{
    auto it = chosen.find(key);
    if (it == chosen.end())
        chosen.insert(key, upload);
    else if (currentHids.contains(upload.hid) && !currentHids.contains(it->hid))
        *it = upload;
}

// one whole volume upload of each volume
static void collectVolumes(const QList<Upload> &uploads, const QSet<QString> &currentHids,
                           QMap<double, Upload> &volumes)
{
    for (const auto &upload : uploads)
        if (upload.isVolume())
            chooseUpload(volumes, upload.volume, upload, currentHids);
}

// chapter numbers that a group uploaded in different volumes
static int restartedNumbers(const QList<Upload> &uploads)
{
    QHash<QString, QString> volumes;
    QSet<QString> restarted;

    for (const auto &upload : uploads)
    {
        if (upload.number <= 0 || upload.vol.isEmpty())
            continue;

        auto key = upload.group + upload.chap;
        auto it = volumes.constFind(key);
        if (it == volumes.constEnd())
            volumes.insert(key, upload.vol);
        else if (it.value() != upload.vol)
            restarted.insert(upload.chap);
    }

    return int(restarted.size());
}

// one upload per volume and chapter number, sorted by volume. uploads without volume are usually
// numbered through all volumes, the ones beyond the chapters in the volumes follow (and prologues
// or oneshots that are in no volume).
static MangaChapterCollection chaptersByVolume(const QString &slug, const QList<Upload> &uploads,
                                               const QSet<QString> &currentHids)
{
    QMap<QPair<double, double>, Upload> inVolumes;
    QMap<double, Upload> withoutVolume;
    QSet<double> numberedVolumes;
    QSet<double> volumeNumbers;

    for (const auto &upload : uploads)
    {
        if (upload.hasVolume)
        {
            chooseUpload(inVolumes, qMakePair(upload.volume, upload.number), upload, currentHids);
            volumeNumbers.insert(upload.number);
            if (upload.number > 0)
                numberedVolumes.insert(upload.volume);
        }
        else
        {
            chooseUpload(withoutVolume, upload.number, upload, currentHids);
        }
    }

    MangaChapterCollection chapters;
    int volumeChapters = 0;

    for (const auto &upload : std::as_const(inVolumes))
    {
        if (upload.number >= 1 && upload.number == std::floor(upload.number))
            volumeChapters++;

        // a whole volume, or the prologue of a volume
        bool wholeVolume = upload.isVolume() && !numberedVolumes.contains(upload.volume);
        chapters.append(makeChapter(slug, upload, !wholeVolume));
    }

    for (const auto &upload : std::as_const(withoutVolume))
        if (upload.number > volumeChapters || (upload.number < 1 && !volumeNumbers.contains(upload.number)))
            chapters.append(makeChapter(slug, upload, true));

    return chapters;
}

// one upload per chapter number, preferably from the given group (like the chapter endpoint does)
static QList<Upload> onePerNumber(const QList<Upload> &uploads, const QString &group)
{
    QList<Upload> entries;
    QHash<QString, int> indices;

    for (const auto &upload : uploads)
    {
        auto it = indices.constFind(upload.rawChap);
        if (it == indices.constEnd())
        {
            indices.insert(upload.rawChap, int(entries.size()));
            entries.append(upload);
        }
        else if (entries[it.value()].group != group && upload.group == group)
        {
            entries[it.value()] = upload;
        }
    }

    return entries;
}

// the upload the chapter endpoint is requested with: its group is preferred for every chapter, the
// others get the first upload of the chapter list. a new list prefers the group with the most
// chapters among the uploads (the first page). an update has to find that group again, otherwise
// most chapters would change: it has the most of the current chapters, and its uploads were chosen
// over other groups' at least as often as not (a group that uploaded the whole series later,
// starting with lower numbers, only lost so far; chapters with loaded pages keep their upload).
static Upload anchorUpload(const QList<Upload> &uploads, const QSet<QString> &currentHids)
{
    QHash<QString, QSet<QString>> groups;
    QHash<QString, QSet<QString>> currentGroups;
    for (const auto &upload : uploads)
    {
        groups[upload.rawChap].insert(upload.group);
        if (currentHids.contains(upload.hid))
            currentGroups[upload.rawChap].insert(upload.group);
    }

    QHash<QString, int> chapters;
    QHash<QString, int> balance;
    for (auto it = groups.cbegin(); it != groups.cend(); ++it)
    {
        if (currentGroups.isEmpty())
        {
            for (const auto &group : it.value())
                chapters[group]++;
            continue;
        }

        auto current = currentGroups.value(it.key());
        if (current.size() != 1)
            continue;

        auto chosen = *current.cbegin();
        chapters[chosen]++;
        if (it.value().size() > 1)
            for (const auto &group : it.value())
                balance[group] += group == chosen ? 1 : -1;
    }

    // the first one of the groups with the most chapters
    int anchor = 0;
    int best = 0;
    for (int i = 0; i < uploads.size(); i++)
    {
        const auto &group = uploads[i].group;
        if (chapters.value(group) > best && balance.value(group) >= 0)
        {
            best = chapters.value(group);
            anchor = i;
        }
    }

    return uploads[anchor];
}

// whether the numbered chapters (sorted) cover a volume: the numbers don't jump before its first
// and after its last chapter
static bool volumeCovered(const QList<Upload> &entries, double volume)
{
    int first = -1;
    int last = -1;
    for (int i = 0; i < entries.size(); i++)
        if (entries[i].number > 0 && entries[i].knownVolume == volume)
        {
            if (first < 0)
                first = i;
            last = i;
        }

    if (first < 0)
        return false;

    double previous = 0;
    for (int i = first - 1; i >= 0 && previous == 0; i--)
        previous = entries[i].number;

    double next = entries[last].number + 1;
    for (int i = last + 1; i < entries.size(); i++)
        if (entries[i].number > 0)
        {
            next = entries[i].number;
            break;
        }

    return entries[first].number - previous <= 1 && next - entries[last].number <= 1;
}

// chapters with loaded pages keep their upload if another upload of them is chosen now
static void keepLoadedUploads(MangaChapterCollection &chapters, const MangaChapterCollection &current)
{
    // e.g. "Ch. 12" or "Vol. 2 Ch. 3", without the title of the upload
    auto key = [](const MangaChapter &chapter) { return chapter.chapterTitle.section(": ", 0, 0); };

    QHash<QString, int> keys;
    QSet<QString> urls;
    for (const auto &chapter : chapters)
    {
        keys[key(chapter)]++;
        urls.insert(chapter.chapterUrl);
    }

    QHash<QString, const MangaChapter *> loaded;
    for (const auto &chapter : current)
        if (chapter.pagesLoaded && !urls.contains(chapter.chapterUrl) && keys.value(key(chapter)) == 1)
            loaded.insert(key(chapter), loaded.contains(key(chapter)) ? nullptr : &chapter);

    for (auto &chapter : chapters)
        if (auto previous = loaded.value(key(chapter)))
        {
            qDebug() << "Comick: keeping loaded upload" << previous->chapterUrl << "instead of"
                     << chapter.chapterUrl;
            chapter.chapterTitle = previous->chapterTitle;
            chapter.chapterUrl = previous->chapterUrl;
        }
}

// appends the uploads of a chapter list page, returns the number of pages
static Result<int, QString> fetchUploads(NetworkManager *networkManager, const QString &url,
                                         QList<Upload> &uploads)
{
    auto job = networkManager->downloadAsString(url, 6000);

    if (!job->await(6000))
        return Err(job->errorString);

    try
    {
        Document doc;
        ParseResult res = doc.Parse(job->buffer.data());

        if (!res || !doc.IsObject() || !doc.HasMember("data") || !doc["data"].IsArray())
            return Err(QString("Couldn't parse chapter list."));

        parseUploads(doc["data"], uploads);

        if (doc.HasMember("pagination") && doc["pagination"].IsObject() &&
            doc["pagination"].HasMember("last_page") && doc["pagination"]["last_page"].IsInt())
            return Ok(doc["pagination"]["last_page"].GetInt());
    }
    catch (const JsonError &)
    {
        return Err(QString("Couldn't parse chapter list."));
    }

    return Ok(1);
}

static QString titleFromSlug(const QString &slug)
{
    static const QRegularExpression prefixrx(R"lit(^(?:0\d|\d{8,})-(?=.))lit");
    static const QRegularExpression idsuffixrx(R"lit(-(?=[a-z0-9]*\d)(?=[a-z0-9]*[a-z])[a-z0-9]{8}$)lit");
    static const QRegularExpression suffixrx(R"lit([A-Z_])lit");

    auto title = slug;

    // slugs of duplicate titles get a random suffix, e.g. "his-starN3nc", "dashu_b0" or
    // "escape-kg4zwqyf"
    if (title.length() > 4 && title.right(4).contains(suffixrx) &&
        !title.left(title.length() - 4).contains(suffixrx))
        title.chop(4);
    title.remove(idsuffixrx);
    title.remove(prefixrx);

    auto words = title.split('-', Qt::SkipEmptyParts);
    for (auto &word : words)
        word[0] = word[0].toUpper();

    return words.isEmpty() ? slug : words.join(' ');
}

static QString decodeXmlEntities(QString str)
{
    if (!str.contains('&'))
        return str;

    return str.replace("&apos;", "'")
        .replace("&quot;", "\"")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&amp;", "&");
}

Comick::Comick(NetworkManager *networkManager) : AbstractMangaSource(networkManager)
{
    name = "Comick";
    baseUrl = "https://comick.art";
    apiUrl = baseUrl + "/api";
    // the comic pages are 150-350 kB
    mangaInfoTimeout = 6000;

    // the image cdn blocks requests without referer
    headerRules.add("comicknew.pictures", "Referer", "https://comick.art/");

    statuses = {"Ongoing", "Completed", "Cancelled", "Hiatus"};
}

Comick::~Comick() = default;

bool Comick::updateMangaList(UpdateProgressToken *token)
{
    // a retry continues with the sitemaps the failed attempt didn't get
    auto &state = token->resumeState<ComickUpdateState>();
    state.mangas.absoluteUrls = false;

    token->sendProgress(token->sourcesProgress.value(name));

    QElapsedTimer timer;
    timer.start();

    if (!state.knownTitlesLoaded)
    {
        for (int i = 0; i < mangaList.size; i++)
            state.knownTitles.insert(mangaList.urls[i], mangaList.titles[i]);
        state.knownTitlesLoaded = true;
    }

    // the most popular and the newest titles come from the search api (real titles),
    // the rest of the catalogue from the sitemaps (only slugs)
    if (!state.searched)
    {
        // the order of the list is the popularity, a retry searches again
        bool ok = fetchSearch("user_follow_count", popularPages, state, token, 0, 18);
        ok = ok && !token->canceled && fetchSearch("created_at", latestPages, state, token, 18, 20);

        if (token->canceled)
        {
            token->sendError(QString("Download cancelled"));
            return false;
        }

        if (!ok)
        {
            token->sendError(QString("Couldn't load the popular titles."));
            return false;
        }

        state.searched = true;
    }

    // a retry continues with the sitemaps that are missing, the list is only taken when it is complete
    if (!fetchSitemaps(state, token))
        return false;

    this->mangaList = state.mangas;

    qDebug() << "mangas:" << state.mangas.size << "time:" << timer.elapsed();

    token->sendProgress(100);

    return true;
}

bool Comick::fetchSearch(const QString &order, int pages, ComickUpdateState &state, UpdateProgressToken *token,
                         int progressFrom, int progressTo)
{
    auto &mangas = state.mangas;
    auto &urls = state.urls;

    QString cursor;

    for (int page = 0; page < pages && !token->canceled; page++)
    {
        auto searchUrl = apiUrl + "/search?order_by=" + order +
                         "&order_direction=desc&type=comic&showAll=false&exclude_mylist=false";
        if (!cursor.isEmpty())
            searchUrl += "&cursor=" + QString::fromLatin1(QUrl::toPercentEncoding(cursor));

        auto job = downloadAsString(searchUrl, 10000);

        if (!job->await(10000))
        {
            qDebug() << "Comick: search failed:" << order << job->errorString;
            return false;
        }

        cursor = "";

        try
        {
            Document doc;
            ParseResult res = doc.Parse(job->buffer.data());

            if (!res || !doc.IsObject() || !doc.HasMember("data") || !doc["data"].IsArray())
                return false;

            for (const auto &r : doc["data"].GetArray())
            {
                auto slug = getStringSafe(r, "slug");
                auto title = getStringSafe(r, "title").trimmed();
                auto url = "/comic/" + slug;

                if (slug.isEmpty() || urls.contains(url))
                    continue;

                urls.insert(url);
                if (title.isEmpty())
                    title = state.knownTitles.value(url, titleFromSlug(slug));
                mangas.append(title, url);
            }

            cursor = getStringSafe(doc, "next_cursor");
        }
        catch (const JsonError &)
        {
            return false;
        }

        token->sendProgress(progressFrom + (progressTo - progressFrom) * (page + 1) / pages);

        if (cursor.isEmpty())
            break;
    }

    return true;
}

bool Comick::fetchSitemaps(ComickUpdateState &state, UpdateProgressToken *token)
{
    QRegularExpression sitemaprx(R"lit(<loc>\s*([^<\s]*/sitemap-comic-(\d+)\.xml)\s*</loc>)lit");

    auto indexJob = downloadToBuffer(baseUrl + "/sitemap.xml", 10000);

    if (!indexJob->await(10000))
    {
        token->sendError(indexJob->errorString);
        return false;
    }

    QMap<int, QString> sitemaps;
    for (auto &match : getAllRxMatches(sitemaprx, QString::fromUtf8(indexJob->buffer)))
        sitemaps.insert(match.captured(2).toInt(), match.captured(1));

    if (sitemaps.isEmpty())
    {
        token->sendError(QString("Couldn't parse manga list."));
        return false;
    }

    for (auto it = sitemaps.cbegin(); it != sitemaps.cend(); ++it)
    {
        if (state.sitemapsDone.contains(it.key()))
            continue;

        auto job = downloadToBuffer(it.value(), 30000);

        if (!job->await(30000))
        {
            token->sendError(job->errorString);
            return false;
        }

        if (token->canceled)
        {
            token->sendError(QString("Download cancelled"));
            return false;
        }

        const auto &buffer = job->buffer;
        int pos = 0;
        while ((pos = buffer.indexOf("<loc>", pos)) >= 0)
        {
            int epos = buffer.indexOf("</loc>", pos);
            if (epos < 0)
                break;

            int spos = buffer.indexOf("/comic/", pos);
            if (spos >= 0 && spos < epos)
            {
                spos += 7;
                auto slug = decodeXmlEntities(QString::fromUtf8(buffer.mid(spos, epos - spos)).trimmed());
                auto url = "/comic/" + slug;

                if (!slug.isEmpty() && !slug.contains('/') && !state.urls.contains(url))
                {
                    state.urls.insert(url);
                    state.mangas.append(state.knownTitles.value(url, titleFromSlug(slug)), url);
                }
            }

            pos = epos;
        }

        state.sitemapsDone.insert(it.key());
        token->sendProgress(qMin(99, 20 + 80 * state.sitemapsDone.size() / sitemaps.size()));
    }

    return true;
}

Result<MangaChapterCollection, QString> Comick::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    // all infos are embedded as json in the comic page
    const auto &buffer = job->buffer;
    int spos = buffer.indexOf(R"(id="comic-data")");
    if (spos >= 0)
        spos = buffer.indexOf('>', spos);
    int epos = spos >= 0 ? buffer.indexOf("</script>", spos) : -1;

    if (spos < 0 || epos < 0)
        return Err(QString("Couldn't parse manga infos."));

    QString slug;

    try
    {
        Document doc;
        auto data = buffer.mid(spos + 1, epos - spos - 1);
        ParseResult res = doc.Parse(data.constData());

        if (!res || !doc.IsObject())
            return Err(QString("Couldn't parse manga infos."));

        slug = getStringSafe(doc, "slug");

        if (doc.HasMember("status") && doc["status"].IsInt())
        {
            int status = doc["status"].GetInt();
            if (status >= 1 && status <= statuses.size())
                info->status = statuses[status - 1];
        }

        if (doc.HasMember("year") && doc["year"].IsInt() && doc["year"].GetInt() > 0)
            info->releaseYear = QString::number(doc["year"].GetInt());

        info->author = joinNames(doc, "authors");
        info->artist = joinNames(doc, "artists");

        QStringList genres;
        auto demographic = getStringSafe(doc, "demographic_name").trimmed();
        if (!demographic.isEmpty())
            genres.append(demographic.left(1).toUpper() + demographic.mid(1));

        if (doc.HasMember("md_comic_md_genres") && doc["md_comic_md_genres"].IsArray())
            for (const auto &entry : doc["md_comic_md_genres"].GetArray())
            {
                if (!entry.IsObject() || !entry.HasMember("md_genres"))
                    continue;

                const auto &genre = entry["md_genres"];
                auto genreName = getStringSafe(genre, "name");
                if (getStringSafe(genre, "group") == "Genre" && !genreName.isEmpty())
                    genres.append(genreName);
            }
        info->genres = genres.join(", ");

        // the description ends with a block of links to the official releases
        auto summary = getStringSafe(doc, "parsed");
        int hrpos = summary.indexOf("<hr");
        if (hrpos >= 0)
            summary.truncate(hrpos);
        summary = htmlToPlainText(summary).trimmed();
        if (summary.isEmpty())
            summary = getStringSafe(doc, "desc").section("\n---", 0, 0).trimmed();
        info->summary = summary;

        auto cover = getStringSafe(doc, "default_thumbnail").trimmed();
        if (!cover.isEmpty())
            info->coverUrl = cover;
    }
    catch (const JsonError &)
    {
        return Err(QString("Couldn't parse manga infos."));
    }

    if (slug.isEmpty())
        return Err(QString("Couldn't parse manga infos."));

    // the core downloads the cover afterwards if it isn't on disk yet (same path). that fails while
    // the cdn is throttled, so it is also downloaded as soon as the warm-up got it into the cache.
    auto coverPath = info->coverPath.isEmpty() ? defaultCoverPath(info) : info->coverPath;
    if (!info->coverUrl.isEmpty() && !QFile::exists(coverPath))
    {
        auto coverWarmUp = startCacheWarmUp(networkManager->networkAccessManager(), info->coverUrl, 8000);
        QWeakPointer<MangaInfo> weakInfo = info;
        coverWarmUp->onCached = [this, weakInfo]()
        {
            if (auto info = weakInfo.toStrongRef())
                downloadCoverAsync(info, false);
        };
    }

    auto chapters = fetchChapters(slug, info->chapters);

    if (chapters.isErr())
        return chapters;

    auto newChapters = chapters.unwrap();
    keepLoadedUploads(newChapters, info->chapters);

    return Ok(newChapters);
}

Result<MangaChapterCollection, QString> Comick::fetchChapters(const QString &slug,
                                                              const MangaChapterCollection &current)
{
    auto listUrl = QString("%1/comics/%2/chapter-list?lang=en&chapOrder=asc").arg(apiUrl, slug);

    // all english uploads of all groups, sorted by chapter number (whole volumes first)
    QList<Upload> uploads;
    auto firstPage = fetchUploads(networkManager, listUrl, uploads);

    if (firstPage.isErr())
        return Err(firstPage.unwrapErr());

    int lastPage = firstPage.unwrap();

    if (uploads.isEmpty())
        return Ok(MangaChapterCollection());

    // an update keeps the uploads of the current chapters where there is a choice
    QSet<QString> currentHids;
    for (const auto &chapter : current)
        currentHids.insert(chapter.chapterUrl.section('/', -1).section("-chapter-", 0, 0));

    // some series start the chapter numbers again in every volume (or season), the chapter
    // endpoint only has one upload per number. they are sorted by volume from all uploads.
    if (lastPage <= maxListPages && restartedNumbers(uploads) >= 3)
    {
        for (int page = 2; page <= lastPage; page++)
        {
            auto res = fetchUploads(networkManager, listUrl + "&page=" + QString::number(page), uploads);
            if (res.isErr())
                return Err(res.unwrapErr());
        }

        return Ok(chaptersByVolume(slug, uploads, currentHids));
    }

    // the chapter endpoint contains the complete list of its language with one entry per chapter
    // number, preferring the scanlation group of the requested chapter
    QList<Upload> entries;
    auto anchor = anchorUpload(uploads, currentHids);
    auto job = downloadAsString(apiUrl + "/comics/" + chapterPath(slug, anchor), 8000);
    bool loaded = job->await(8000);

    if (loaded)
    {
        try
        {
            Document doc;
            ParseResult res = doc.Parse(job->buffer.data());

            loaded = res && doc.IsObject() && doc.HasMember("chapterList") && doc["chapterList"].IsArray();
            if (loaded)
                parseUploads(doc["chapterList"], entries);
        }
        catch (const JsonError &)
        {
            loaded = false;
        }
    }

    if (loaded)
    {
        // the list comes newest first
        QList<Upload> oldestFirst;
        QSet<QString> hids;
        for (const auto &entry : std::as_const(entries))
            if (!hids.contains(entry.hid))
            {
                hids.insert(entry.hid);
                oldestFirst.prepend(entry);
            }
        entries = oldestFirst;
    }
    else if (lastPage == 1 && current.isEmpty())
    {
        // all uploads are known already. an update keeps its current list instead, the choice of
        // the uploads can differ from the chapter endpoint.
        qDebug() << "Comick: chapter list from the uploads:" << job->errorString;
        entries = onePerNumber(uploads, anchor.group);
    }
    else
    {
        return Err(job->errorString.isEmpty() ? QString("Couldn't parse chapter list.") : job->errorString);
    }

    std::stable_sort(entries.begin(), entries.end(),
                     [](const Upload &a, const Upload &b) { return a.number < b.number; });

    QMap<double, Upload> volumes;
    collectVolumes(uploads, currentHids, volumes);

    // volumes of the numbered chapters, other uploads of a chapter often have one
    QHash<QString, double> uploadVolumes;
    for (const auto &upload : std::as_const(uploads))
        if (upload.knownVolume >= 0 && !uploadVolumes.contains(upload.chap))
            uploadVolumes.insert(upload.chap, upload.knownVolume);

    bool numbered = false;
    bool allVolumesKnown = true;
    double maxNumber = 0;
    QSet<double> chapterVolumes;

    for (auto &entry : entries)
    {
        if (entry.number <= 0)
            continue;

        if (entry.knownVolume < 0)
            entry.knownVolume = uploadVolumes.value(entry.chap, -1);

        numbered = true;
        maxNumber = qMax(maxNumber, entry.number);
        if (entry.knownVolume >= 0)
            chapterVolumes.insert(entry.knownVolume);
        else
            allVolumesKnown = false;
    }

    MangaChapterCollection chapters;

    // whole volumes are uploaded as chapter 0, the list only contains an arbitrary one of them.
    // series that are only uploaded as volumes get all of them from the chapter list pages (sorted
    // by chapter, so the volumes come first). so do series whose few chapters can't cover the
    // volumes, e.g. only the start of the first volume.
    if (!volumes.isEmpty() &&
        (!numbered ||
         (volumes.size() > 1 && chapterVolumes.isEmpty() && maxNumber < 4 * volumes.lastKey())))
    {
        for (int page = 2; page <= qMin(lastPage, maxListPages); page++)
        {
            auto res = fetchUploads(networkManager, listUrl + "&page=" + QString::number(page), uploads);
            if (res.isErr())
            {
                if (!current.isEmpty())
                    return Err(res.unwrapErr());
                break;
            }
        }
        collectVolumes(uploads, currentHids, volumes);

        for (const auto &entry : std::as_const(entries))
        {
            if (entry.number > 0)
                continue;

            // e.g. oneshots
            if (!entry.isVolume())
                chapters.append(makeChapter(slug, entry));
            else if (!volumes.contains(entry.volume))
                volumes.insert(entry.volume, entry);
        }

        for (const auto &volume : std::as_const(volumes))
            chapters.append(makeChapter(slug, volume));

        return Ok(chapters);
    }

    // next to numbered chapters the volumes are dropped, except the first one (often a prologue)
    QList<Upload> result;
    QSet<double> listedVolumes;

    for (const auto &entry : std::as_const(entries))
    {
        if (entry.isVolume())
        {
            if (numbered && entry.volume > 1)
                continue;
            listedVolumes.insert(entry.volume);
        }
        result.append(entry);
        chapters.append(makeChapter(slug, entry));
    }

    // volumes whose chapters are missing (e.g. only the first volume is translated chapter by
    // chapter) are inserted before the chapters of the following volumes
    if (numbered && allVolumesKnown)
        for (auto it = volumes.cbegin(); it != volumes.cend(); ++it)
        {
            if (listedVolumes.contains(it.key()) || volumeCovered(entries, it.key()))
                continue;

            int pos = 0;
            while (pos < result.size() && !(result[pos].number > 0 && result[pos].knownVolume > it.key()))
                pos++;
            result.insert(pos, it.value());
            chapters.insert(pos, makeChapter(slug, it.value()));
        }

    return Ok(chapters);
}

Result<QList<ImageLink>, QString> Comick::getPageList(const QString &chapterUrl)
{
    auto job = downloadAsString(apiUrl + "/comics/" + chapterUrl, 8000);

    if (!job->await(8000))
        return Err(job->errorString);

    QStringList imageUrls;

    try
    {
        Document doc;
        ParseResult res = doc.Parse(job->buffer.data());

        if (!res || !doc.IsObject() || !doc.HasMember("chapter") || !doc["chapter"].IsObject())
            return Err(QString("Couldn't parse page list."));

        const auto &chapter = doc["chapter"];

        if (chapter.HasMember("images") && chapter["images"].IsArray())
            for (const auto &image : chapter["images"].GetArray())
            {
                auto imageUrl = getStringSafe(image, "url");
                if (!imageUrl.isEmpty())
                    imageUrls.append(imageUrl);
            }
    }
    catch (const JsonError &)
    {
        return Err(QString("Couldn't parse page list."));
    }

    // remembered for warming up the following pages
    if (nextPages.size() > 5000)
        nextPages.clear();
    for (int i = 0; i + 1 < imageUrls.size(); i++)
        nextPages.insert(imageUrls[i], imageUrls[i + 1]);

    return Ok(ImageLink::fromUrls(imageUrls));
}

Result<ImageLink, QString> Comick::getImageLink(const ImageLink &page)
{
    const QString &pageUrl = page.url;
    auto networkAccessManager = networkManager->networkAccessManager();

    // the following pages are usually requested next
    auto next = pageUrl;
    for (int i = 0; i < prefetchPages && nextPages.contains(next); i++)
    {
        next = nextPages.value(next);
        if (!pageWarmUps.contains(next))
            pageWarmUps.insert(next, startCacheWarmUp(networkAccessManager, next, 8000));
    }

    auto warmUp = pageWarmUps.take(pageUrl);
    if (!warmUp || (warmUp->finished && !warmUp->cached))
        warmUp = startCacheWarmUp(networkAccessManager, pageUrl, 5000);

    awaitCacheWarmUp(warmUp, 5000);

    // warm-ups of skipped pages
    if (pageWarmUps.size() > 32)
    {
        for (auto it = pageWarmUps.begin(); it != pageWarmUps.end();)
        {
            if (it.value()->finished)
                it = pageWarmUps.erase(it);
            else
                ++it;
        }
    }

    return Ok(page);
}
