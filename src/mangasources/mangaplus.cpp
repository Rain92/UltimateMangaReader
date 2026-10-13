#include "mangaplus.h"

#include <QUuid>

#include <algorithm>
#include <limits>
#include <utility>

namespace
{
// minimal bounds checked reader for the protobuf wire format of the api responses
class ProtoMessage
{
public:
    ProtoMessage() = default;
    explicit ProtoMessage(const QByteArray &data) : ProtoMessage(data, 0, data.size()) {}

    bool has(int number) const
    {
        return std::any_of(fields.begin(), fields.end(),
                           [number](const Field &f) { return f.number == number; });
    }

    quint64 getInt(int number, quint64 defaultValue = 0) const
    {
        auto field = find(number, WireVarint);
        return field ? field->value : defaultValue;
    }

    QString getString(int number) const
    {
        auto field = find(number, WireLengthDelimited);
        return field ? QString::fromUtf8(buffer.constData() + field->offset, field->length) : QString();
    }

    ProtoMessage getMessage(int number) const
    {
        auto field = find(number, WireLengthDelimited);
        return field ? ProtoMessage(buffer, field->offset, field->length) : ProtoMessage();
    }

    QList<ProtoMessage> getMessages(int number) const
    {
        QList<ProtoMessage> messages;
        for (const auto &field : fields)
            if (field.number == number && field.wireType == WireLengthDelimited)
                messages.append(ProtoMessage(buffer, field.offset, field.length));
        return messages;
    }

private:
    enum WireType
    {
        WireVarint = 0,
        WireFixed64 = 1,
        WireLengthDelimited = 2,
        WireFixed32 = 5
    };

    struct Field
    {
        int number;
        int wireType;
        quint64 value;
        int offset;
        int length;
    };

    QByteArray buffer;
    QVector<Field> fields;

    // invalid or truncated data results in an empty message
    ProtoMessage(const QByteArray &data, int offset, int length) : buffer(data)
    {
        if (!parse(offset, offset + length))
            fields.clear();
    }

    const Field *find(int number, int wireType) const
    {
        for (const auto &field : fields)
            if (field.number == number && field.wireType == wireType)
                return &field;
        return nullptr;
    }

    bool readVarint(int &pos, int end, quint64 &value) const
    {
        auto data = reinterpret_cast<const uchar *>(buffer.constData());
        value = 0;
        for (int shift = 0; shift < 64 && pos < end; shift += 7)
        {
            uchar byte = data[pos++];
            value |= quint64(byte & 0x7f) << shift;
            if (byte < 0x80)
                return true;
        }
        return false;
    }

    bool parse(int pos, int end)
    {
        while (pos < end)
        {
            quint64 tag;
            if (!readVarint(pos, end, tag) || (tag >> 3) == 0 || (tag >> 3) > 0x1fffffff)
                return false;

            Field field{int(tag >> 3), int(tag & 7), 0, pos, 0};

            if (field.wireType == WireVarint)
            {
                if (!readVarint(pos, end, field.value))
                    return false;
            }
            else if (field.wireType == WireLengthDelimited)
            {
                quint64 size;
                if (!readVarint(pos, end, size) || size > quint64(end - pos))
                    return false;
                field.offset = pos;
                field.length = int(size);
                pos += field.length;
            }
            else if (field.wireType == WireFixed64 || field.wireType == WireFixed32)
            {
                int size = field.wireType == WireFixed64 ? 8 : 4;
                if (end - pos < size)
                    return false;
                pos += size;
            }
            else
            {
                return false;
            }

            fields.append(field);
        }
        return true;
    }
};

//    message Response {
//      SuccessResult success = 1;
//      ErrorResult error = 2;   // ErrorResult { Popup englishPopup = 2; } Popup { subject = 1; body = 2; }
//    }
Result<ProtoMessage, QString> parseResponse(const QByteArray &buffer)
{
    ProtoMessage response(buffer);

    if (response.has(1))
        return Ok(response.getMessage(1));

    auto popup = response.getMessage(2).getMessage(2);
    auto subject = popup.getString(1).trimmed();
    auto body = popup.getString(2).trimmed();

    if (subject.isEmpty() && body.isEmpty())
        return Err(QString("Couldn't parse MangaPlus response."));

    return Err(subject.isEmpty() || body.isEmpty() ? subject + body : subject + ": " + body);
}

// chapter names are "#001", "ex" or "One-shot", the subtitles usually contain the number
QString chapterTitle(const QString &name, const QString &subTitle)
{
    auto number = name.trimmed();
    auto title = subTitle.trimmed();

    if (title.isEmpty())
        return number;

    QRegularExpression numberrx(R"lit(^#0*(\d+)$)lit");
    auto match = numberrx.match(number);
    if (match.hasMatch() &&
        !QRegularExpression("(?<!\\d)" + match.captured(1) + "(?!\\d)").match(title).hasMatch())
        return number + " - " + title;

    return title;
}

QString titleStatus(const ProtoMessage &detail, const QStringList &genres, int chapterCount)
{
    // TitleLabels.releaseSchedule: 1-6 regular releases, 7 other, 8 completed, 9 one-shot, 10 hiatus
    auto schedule = detail.getMessage(32).getInt(1);

    if (schedule == 8)
        return "Completed";
    if (schedule == 9 || genres.contains("One-shot", Qt::CaseInsensitive))
        return "One-shot";
    if (schedule == 10)
        return "Hiatus";
    if (schedule >= 1 && schedule <= 6)
        return "Ongoing";

    auto nonAppearanceInfo = detail.getString(8);
    if (nonAppearanceInfo.contains("complete", Qt::CaseInsensitive))
        return "Completed";
    if (nonAppearanceInfo.contains("hiatus", Qt::CaseInsensitive))
        return "Hiatus";

    // award titles have no release schedule
    if (chapterCount == 1)
        return "One-shot";

    return "";
}

qint64 urlExpiry(const QString &url)
{
    static const QRegularExpression expiresrx(R"lit([?&]expires=(\d+))lit");

    auto match = expiresrx.match(url);
    return match.hasMatch() ? match.captured(1).toLongLong() : std::numeric_limits<qint64>::max();
}

QString urlViewToken(const QString &url)
{
    static const QRegularExpression tokenrx(R"lit(#vwt=([^|]+))lit");

    auto match = tokenrx.match(url);
    return match.hasMatch() ? match.captured(1) : QString();
}

// the part of a page url that is sent to the image host
QString requestUrl(const QString &pageUrl)
{
    return pageUrl.section('|', 0, 0).section('#', 0, 0);
}

QString findImageUrl(const QStringList &imageUrls, const QString &fileName)
{
    for (const auto &url : imageUrls)
        if (url.contains(fileName))
            return url;

    return QString();
}

}  // namespace

MangaPlus::MangaPlus(NetworkManager *dm) : AbstractMangaSource(dm)
{
    name = "MangaPlus";
    baseUrl = "https://jumpg-webapi.tokyo-cdn.com/";
    mangalistUrl = "api/title_list/allV2";
    rankingUrl = "api/title_list/rankingV2?lang=eng&type=%1&clang=eng";
    titleDetailUrl = "api/title_detailV3?title_id=%1&clang=eng";
    pagesUrl = "api/manga_viewer_v3?chapter_id=%1&split=yes&img_quality=super_high&clang=eng";

    // the api server is far away (~300 ms rtt), a new connection often takes longer than 2 s
    mangaInfoTimeout = 6000;

    // like the web client: without a session token the api answers with an "Account Banned" error
    headerRules.add("jumpg-webapi.tokyo-cdn.com", "SESSION-TOKEN",
                    QUuid::createUuid().toString(QUuid::WithoutBraces).toLatin1());
    headerRules.add("tokyo-cdn.com", "Origin", "https://mangaplus.shueisha.co.jp");
    headerRules.add("tokyo-cdn.com", "Referer", "https://mangaplus.shueisha.co.jp/");

    QObject::connect(networkManager->networkAccessManager(), &QNetworkAccessManager::finished, &replyContext,
                     [this](QNetworkReply *reply) { forgetFailedImageUrl(reply); });
}

HttpHeaders MangaPlus::requestHeaders(const QString &url) const
{
    auto headers = AbstractMangaSource::requestHeaders(url);

    // the image host only accepts the view token of the fetch the image urls came from, it is kept in the
    // fragment of their urls (which isn't sent to the server)
    auto viewToken = urlViewToken(url);
    if (!viewToken.isEmpty())
        headers.append({"Plus-Vw-Token", viewToken.toLatin1()});

    return headers;
}

QString MangaPlus::normalizeMangaUrl(const QString &url) const
{
    // older versions stored api/title_detail?title_id=<id>
    QRegularExpression titleidrx(R"lit((?:title_id=|/titles/)(\d+))lit");

    auto match = titleidrx.match(url);
    if (!match.hasMatch())
        return url;

    return baseUrl + titleDetailUrl.arg(match.captured(1));
}

QString MangaPlus::normalizeChapterUrl(const QString &url) const
{
    // older versions stored api/manga_viewer?chapter_id=<id>
    QRegularExpression chapteridrx(R"lit((?:chapter_id=|/viewer/)(\d+))lit");

    auto match = chapteridrx.match(url);
    if (!match.hasMatch())
        return url;

    return pagesUrl.arg(match.captured(1));
}

// image urls expire one to two hours after they were loaded (at the start of the hour after next) and their
// view token is only sent in the session that loaded them
bool MangaPlus::isUsableImageUrl(const QString &url) const
{
    // the download manager resolves the urls of all chapters before it downloads the images
    const qint64 margin = 600;

    auto token = urlViewToken(url);
    return urlExpiry(url) > QDateTime::currentSecsSinceEpoch() + margin &&
           (token.isEmpty() || viewTokens.contains(token));
}

// the stored infos contain the urls of older versions of this source and resolved image urls that can't be
// used any more. the core only resolves empty image urls again, the ones of downloaded pages are kept,
// so these pages can still be read without a network connection
void MangaPlus::cleanUpStoredUrls(QSharedPointer<MangaInfo> info)
{
    info->url = normalizeMangaUrl(info->url);

    for (int c = 0; c < info->chapters.count(); c++)
    {
        auto &chapter = info->chapters[c];
        chapter.chapterUrl = normalizeChapterUrl(chapter.chapterUrl);

        for (int p = 0; p < chapter.imageUrlList.count(); p++)
        {
            auto &imageUrl = chapter.imageUrlList[p];
            if (imageUrl != "" && !isUsableImageUrl(imageUrl) &&
                !QFile::exists(getImagePath(DownloadImageDescriptor("", info->title, c, p))))
                imageUrl = "";
        }
    }
}

void MangaPlus::trackInfo(QSharedPointer<MangaInfo> info)
{
    openedInfos.erase(std::remove_if(openedInfos.begin(), openedInfos.end(),
                                     [&info](const QWeakPointer<MangaInfo> &weakInfo)
                                     {
                                         auto trackedInfo = weakInfo.toStrongRef();
                                         return !trackedInfo || trackedInfo == info;
                                     }),
                      openedInfos.end());
    openedInfos.append(info);
}

// the core keeps a resolved image url until its page is downloaded, even after the url expired. a url that
// couldn't be downloaded is forgotten, so it is resolved (and refreshed if needed) again the next time
void MangaPlus::forgetFailedImageUrl(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError || !reply->url().host().endsWith("tokyo-cdn.com") ||
        !reply->url().path().contains("/manga_page/"))
        return;

    auto failedUrl = reply->url().toString(QUrl::RemoveFragment);

    for (const auto &weakInfo : std::as_const(openedInfos))
    {
        auto info = weakInfo.toStrongRef();
        if (!info)
            continue;

        for (int c = 0; c < info->chapters.count(); c++)
            for (int p = 0; p < info->chapters.at(c).imageUrlList.count(); p++)
                if (requestUrl(info->chapters.at(c).imageUrlList.at(p)) == failedUrl)
                    info->chapters[c].imageUrlList[p] = "";
    }
}

bool MangaPlus::updateMangaList(UpdateProgressToken *token)
{
    auto job = downloadToBuffer(baseUrl + mangalistUrl, 20000);

    if (!job->await(20000))
    {
        token->sendError(job->errorString);
        return false;
    }

    if (token->canceled)
        return false;

    token->sendProgress(40);

    auto successR = parseResponse(job->buffer);
    if (successR.isErr())
    {
        token->sendError(successR.unwrapErr());
        return false;
    }

    //    message AllTitlesViewV2 { repeated AllTitlesGroup groups = 1; }
    //    message AllTitlesGroup { string theTitle = 1; repeated Title titles = 2; }   // one per language
    //    message Title {
    //      uint32 titleId = 1;
    //      string name = 2;
    //      string author = 3;
    //      string portraitImageUrl = 4;
    //      int32 language = 7;   // 0 = english (omitted)
    //    }
    QList<quint64> titleIds;
    QHash<quint64, QString> titles;

    for (const auto &group : successR.unwrap().getMessage(25).getMessages(1))
    {
        for (const auto &title : group.getMessages(2))
        {
            auto titleId = title.getInt(1);
            auto titleName = title.getString(2).trimmed();

            if (title.getInt(7) != 0 || titleId == 0 || titleName.isEmpty() || titles.contains(titleId))
                continue;

            titleIds.append(titleId);
            titles.insert(titleId, titleName);
        }
    }

    if (titleIds.isEmpty())
    {
        token->sendError("Couldn't parse the manga list.");
        return false;
    }

    // popularity order: hottest and completed rankings first, then the rest in A-Z order
    //    message TitleRankingViewV2 { repeated TitleRankingGroup rankedTitles = 3; }
    //    message TitleRankingGroup { uint32 originalTitleId = 1; repeated Title titles = 2; }
    QList<quint64> orderedIds;
    QSet<quint64> added;
    int progress = 40;

    for (const auto &type : QStringList{"hottest", "completed"})
    {
        auto rankingJob = downloadToBuffer(baseUrl + rankingUrl.arg(type), 10000);

        bool ok = rankingJob->await(10000);

        if (token->canceled)
            return false;

        token->sendProgress(progress += 25);

        if (!ok)
        {
            qDebug() << "MangaPlus: couldn't load ranking" << type << rankingJob->errorString;
            continue;
        }

        auto rankingR = parseResponse(rankingJob->buffer);
        if (rankingR.isErr())
        {
            qDebug() << "MangaPlus: couldn't load ranking" << type << rankingR.unwrapErr();
            continue;
        }

        for (const auto &group : rankingR.unwrap().getMessage(37).getMessages(3))
        {
            for (const auto &title : group.getMessages(2))
            {
                auto titleId = title.getInt(1);
                if (titles.contains(titleId) && !added.contains(titleId))
                {
                    added.insert(titleId);
                    orderedIds.append(titleId);
                }
            }
        }
    }

    for (auto titleId : std::as_const(titleIds))
        if (!added.contains(titleId))
            orderedIds.append(titleId);

    MangaList mangas;
    mangas.absoluteUrls = false;

    for (auto titleId : std::as_const(orderedIds))
        mangas.append(titles[titleId], titleDetailUrl.arg(titleId));

    this->mangaList = mangas;

    token->sendProgress(100);

    return true;
}

// favorites and manga lists of older versions contain api/title_detail urls
Result<QSharedPointer<MangaInfo>, QString> MangaPlus::getMangaInfo(const QString &mangaUrl,
                                                                   const QString &mangaTitle)
{
    auto infoR = AbstractMangaSource::getMangaInfo(normalizeMangaUrl(mangaUrl), mangaTitle);

    if (infoR.isOk())
        trackInfo(infoR.unwrap());

    return infoR;
}

// called for every stored info before it is used (opened from the manga list or shown in the favorites)
void MangaPlus::updateMangaInfoAsync(QSharedPointer<MangaInfo> info, bool updateCover)
{
    trackInfo(info);
    cleanUpStoredUrls(info);

    AbstractMangaSource::updateMangaInfoAsync(info, updateCover);
}

Result<MangaChapterCollection, QString> MangaPlus::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    auto successR = parseResponse(job->buffer);
    if (successR.isErr())
        return Err(successR.unwrapErr());

    //    message TitleDetailView {
    //      Title title = 1;
    //      string titleImageUrl = 2;
    //      string overview = 3;
    //      string viewingPeriodDescription = 7;
    //      string nonAppearanceInfo = 8;
    //      repeated ChapterGroup chapterListGroup = 28;
    //      repeated Tag tags = 31;   // Tag { string tag = 1; string slug = 2; }
    //      TitleLabels titleLabels = 32;   // TitleLabels { int32 releaseSchedule = 1; }
    //    }
    auto detail = successR.unwrap().getMessage(8);
    auto title = detail.getMessage(1);

    if (!title.has(1))
        return Err(QString("Couldn't parse manga info."));

    QStringList authors;
    for (const auto &author : title.getString(3).split('/', Qt::SkipEmptyParts))
        if (!author.trimmed().isEmpty())
            authors.append(author.trimmed());

    info->author = authors.join(", ");
    info->artist = info->author;

    QStringList genres;
    for (const auto &tag : detail.getMessages(31))
        if (!tag.getString(1).trimmed().isEmpty())
            genres.append(tag.getString(1).trimmed());

    info->genres = genres.join(", ");

    QStringList summary;
    for (int field : {3, 8, 7})
    {
        auto text = detail.getString(field).trimmed();
        if (!text.isEmpty() && !text.contains("latest 0 chapters"))
            summary.append(text);
    }

    auto coverUrl = title.getString(4);
    if (coverUrl.isEmpty())
        coverUrl = detail.getString(2);
    if (!coverUrl.isEmpty())
        info->coverUrl = coverUrl;

    //    message ChapterGroup {
    //      string chapterNumbers = 1;
    //      repeated Chapter firstChapterList = 2;
    //      repeated Chapter midChapterList = 3;   // only readable with a subscription
    //      repeated Chapter lastChapterList = 4;
    //    }
    //    message Chapter {
    //      uint32 titleId = 1;
    //      uint32 chapterId = 2;
    //      string name = 3;
    //      string subTitle = 4;
    //      uint32 startTimeStamp = 6;
    //      uint32 endTimeStamp = 7;
    //    }
    QList<ProtoMessage> chapters;
    QSet<quint64> allChapterIds;
    bool hasSubscriptionChapters = false;
    for (const auto &group : detail.getMessages(28))
    {
        for (int list : {2, 3, 4})
        {
            for (const auto &chapter : group.getMessages(list))
            {
                allChapterIds.insert(chapter.getInt(2));
                if (list == 3)
                    hasSubscriptionChapters = true;
                else
                    chapters.append(chapter);
            }
        }
    }

    info->status = titleStatus(detail, genres, allChapterIds.size());

    MangaChapterCollection newchapters;
    QSet<quint64> chapterIds;

    for (const auto &chapter : std::as_const(chapters))
    {
        auto chapterId = chapter.getInt(2);

        // expired chapters have no subtitle
        if (chapterId == 0 || !chapter.has(4) || chapterIds.contains(chapterId))
            continue;

        chapterIds.insert(chapterId);
        newchapters.append(
            MangaChapter(chapterTitle(chapter.getString(3), chapter.getString(4)), pagesUrl.arg(chapterId)));
    }

    if (newchapters.isEmpty() && hasSubscriptionChapters)
        summary.append("The chapters of this title are only readable with a MANGA Plus subscription.");

    info->summary = summary.join("\n\n");

    return Ok(newchapters);
}

Result<QStringList, QString> MangaPlus::getPageList(const QString &chapterUrl)
{
    auto url = normalizeChapterUrl(chapterUrl);
    if (!url.startsWith("http"))
        url.prepend(baseUrl);

    auto job = downloadToBuffer(url, 7000);

    if (!job->await(7000))
        return Err(job->errorString);

    auto successR = parseResponse(job->buffer);
    if (successR.isErr())
        return Err(successR.unwrapErr());

    //    message MangaViewer {
    //      repeated Page pages = 1;
    //      uint32 chapterId = 2;
    //      string vwToken = 19;
    //    }
    //    message Page { MangaPage mangaPage = 1; ... }   // other fields are banners and ads
    //    message MangaPage {
    //      string imageUrl = 1;
    //      uint32 width = 2;
    //      uint32 height = 3;
    //      string encryptionKey = 5;
    //    }
    auto viewer = successR.unwrap().getMessage(10);

    // the image host only accepts the view token of the fetch the image urls came from, it is kept in the url
    // fragment (not sent to the server), see requestHeaders()
    auto viewToken = viewer.getString(19);
    QString fragment;
    if (!viewToken.isEmpty())
    {
        fragment = "#vwt=" + viewToken;
        viewTokens.insert(viewToken);
    }

    QStringList imageUrls;
    for (const auto &page : viewer.getMessages(1))
    {
        auto mangaPage = page.getMessage(1);
        auto imageUrl = mangaPage.getString(1);
        if (imageUrl.isEmpty())
            continue;

        auto key = mangaPage.getString(5);
        imageUrls.append(imageUrl + fragment + (key.isEmpty() ? QString() : "|xor:" + key));
    }

    return Ok(imageUrls);
}

// page urls are the image urls of the page list. expired ones and the ones loaded in an earlier session are
// replaced by the urls of a fresh page list of their chapter
Result<QString, QString> MangaPlus::getImageUrl(const QString &pageUrl)
{
    QRegularExpression pagerx(R"lit(/chapter/(\d+)/manga_page/(?:[^/?#|]*/)*([^/?#|]+)\?)lit");

    auto match = pagerx.match(pageUrl);
    if (!match.hasMatch() || isUsableImageUrl(pageUrl))
        return Ok(pageUrl);

    auto chapterId = match.captured(1);
    auto fileName = "/" + match.captured(2) + "?";

    if (refreshedPageLists.size() > 16 && !refreshedPageLists.contains(chapterId))
        refreshedPageLists.clear();

    const auto refreshed = refreshedPageLists.value(chapterId);
    auto url = findImageUrl(refreshed.imageUrls, fileName);

    // At most one request per chapter and minute: neither a failing network nor a wrong device clock
    // may cause a request (and a timeout) per page. Pages that are needed while the request runs (in an
    // event loop, it can't be waited for) make their own one.
    auto now = QDateTime::currentSecsSinceEpoch();
    if ((url.isEmpty() || !isUsableImageUrl(url)) &&
        (now - refreshed.time >= 60 || runningRefreshes.value(chapterId) > 0))
    {
        refreshedPageLists[chapterId].time = now;

        runningRefreshes[chapterId]++;
        auto pagesR = getPageList(pagesUrl.arg(chapterId));
        if (--runningRefreshes[chapterId] == 0)
            runningRefreshes.remove(chapterId);

        if (pagesR.isErr())
            return Err(pagesR.unwrapErr());

        // the hash can have changed meanwhile, nothing refers into it across the download
        refreshedPageLists[chapterId] = {now, pagesR.unwrap()};
        url = findImageUrl(refreshedPageLists[chapterId].imageUrls, fileName);
    }

    if (url.isEmpty())
        return Err(QString("Couldn't refresh the expired page url."));

    return Ok(url);
}
