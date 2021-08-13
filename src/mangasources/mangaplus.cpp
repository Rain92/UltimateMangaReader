#include "mangaplus.h"

MangaPlus::MangaPlus(NetworkManager *dm) : AbstractMangaSource(dm)
{
    name = "MangaPlus";
    baseUrl = "https://jumpg-webapi.tokyo-cdn.com/";
    mangalistUrl = "api/title_list/all";
    chapterDetailUrl = "api/title_detail?title_id=%1";
    pagesUrl = "api/manga_viewer?chapter_id=%1&img_quality=super_high&split=yes";

    invalidatePagelist();
}

// encrypiton keys are changing almost daily, we need to invalidate the old ones
void MangaPlus::invalidatePagelist()
{
    auto path = CONF.mangasourcedir(this->name);
    QDir dir(path);
    QDir::Filters dirFilters = QDir::Dirs | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden;

    for (const auto &mangadir : dir.entryList(dirFilters))
    {
        auto mangaPath = CONF.mangainfodir(name, mangadir) + "mangainfo.dat";
        auto mi = MangaInfo::deserialize(this, mangaPath);
        for (int i = 0; i < mi->chapters.size(); i++)
        {
            if (!mi->chapters[i].pagesLoaded)
                continue;

            for (int c = 0; c < mi->chapters[i].imageUrlList.size(); c++)
                mi->chapters[i].imageUrlList[c] = "";
        }
        mi->serialize();
    }
}

bool MangaPlus::updateMangaList(UpdateProgressToken *token)
{
    auto job = networkManager->downloadToBuffer(baseUrl + mangalistUrl);

    if (!job->await(7000))
    {
        token->sendError(job->errorString);
        return false;
    }
    picoproto::Message message;

    token->sendProgress(50);
    message.ParseFromBytes((uint8_t *)job->buffer.data(), job->buffer.size());

    if (!message.CheckFieldForType(1, picoproto::FIELD_BYTES))
        return false;

    auto titles = message.GetMessage(1)->GetMessage(5)->GetMessageArray(1);
    //        message Title {
    //          optional uint32 titleId = 1;
    //          optional string name = 2;
    //          optional string author = 3;
    //          optional string portraitImageUrl = 4;
    //          optional string landscapeImageUrl = 5;
    //          optional uint32 viewCount = 6;
    //          optional int32 language = 7;
    //        }

    std::sort(titles.begin(), titles.end(), [](auto a, auto b) { return a->GetUInt64(6) > b->GetUInt64(6); });

    MangaList mangas;
    mangas.absoluteUrls = false;

    for (const auto &t : titles)
    {
        auto titleid = t->GetUInt64(1);
        auto title = QString::fromUtf8(t->GetString(2).c_str());
        auto url = chapterDetailUrl.arg(titleid);

        if (t->CheckFieldForType(7, picoproto::FIELD_UINT64))
        {
            auto language = t->GetUInt64(7);
            if (language > 0)
                continue;
        }

        mangas.append(title, url);
    }

    this->mangaList = mangas;

    token->sendProgress(100);

    return true;
}

Result<MangaChapterCollection, QString> MangaPlus::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    picoproto::Message message;

    message.ParseFromBytes((uint8_t *)job->buffer.data(), job->buffer.size());

    if (!message.CheckFieldForType(1, picoproto::FIELD_BYTES))
        return Err(QString("Error updating manga."));

    auto detail = message.GetMessage(1)->GetMessage(8);
    //    message TitleDetailView {
    //      optional Title title = 1;
    //      optional string titleImageUrl = 2;
    //      optional string overview = 3;
    //      optional string backgroundImageUrl = 4;
    //      optional uint32 nextTimeStamp = 5;
    //      optional int32 updateTiming = 6;
    //      optional string viewingPeriodDescription = 7;
    //      optional string nonAppearanceInfo = 8;
    //      repeated Chapter firstChapterList = 9;
    //      repeated Chapter lastChapterList = 10;
    //      repeated Banner banners = 11;
    //      repeated Title recommendedTitleList = 12;
    //      optional Sns sns = 13;
    //      optional bool isSimulReleased = 14;
    //      optional bool isSubscribed = 15;
    //      optional int32 rating = 16;
    //      optional bool chaptersDescending = 17;
    //      optional uint32 numberOfViews = 18;
    //    }

    auto title = detail->GetMessage(1);
    auto author = QString::fromUtf8(title->GetString(3).c_str());
    auto coverurl = QString::fromUtf8(title->GetString(4).c_str());
    auto summary = QString::fromUtf8(detail->GetString(3).c_str());

    info->author = author;
    info->coverUrl = coverurl;
    info->summary = summary;

    auto chapters = detail->GetMessageArray(9);

    auto chaptersL = detail->GetMessageArray(10);
    chapters.insert(chapters.end(), chaptersL.begin(), chaptersL.end());
    std::reverse(chapters.begin(), chapters.end());

    //    message Chapter {
    //      optional uint32 titleId = 1;
    //      optional uint32 chapterId = 2;
    //      optional string name = 3;
    //      optional string subTitle = 4;
    //      optional string thumbnailUrl = 5;
    //      optional uint32 startTimeStamp = 6;
    //      optional uint32 endTimeStamp = 7;
    //      optional bool alreadyViewed = 8;
    //      optional bool isVerticalOnly = 9;
    //    }

    MangaChapterCollection newchapters;
    for (const auto &c : chapters)
    {
        auto chapterName = QString::fromUtf8(c->GetString(4).c_str());
        auto chapterId = c->GetUInt64(2);
        auto chapterUrl = pagesUrl.arg(chapterId);

        newchapters.insert(0, MangaChapter(chapterName, chapterUrl));
    }

    return Ok(newchapters);
}

Result<QStringList, QString> MangaPlus::getPageList(const QString &chapterUrl)
{
    auto job = networkManager->downloadToBuffer(baseUrl + chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    picoproto::Message message;
    message.ParseFromBytes((uint8_t *)job->buffer.data(), job->buffer.size());

    if (!message.CheckFieldForType(1, picoproto::FIELD_BYTES))
        return Err(QString("Couldn't parse page list!"));

    auto page = message.GetMessage(1)->GetMessage(10);

    //    message MangaViewer {
    //      repeated Page pages = 1;
    //      optional uint32 chapterId = 2;
    //      repeated Chapter chapters = 3;
    //      optional Sns sns = 4;
    //      optional string titleName = 5;
    //      optional string chapterName = 6;
    //      optional uint32 numberOfComments = 7;
    //      optional bool isVerticalOnly = 8;
    //      optional uint32 titleId = 9;
    //      optional bool startFromRight = 10;
    //    }

    //    message MangaPage {
    //      optional string imageUrl = 1;
    //      optional uint32 width = 2;
    //      optional uint32 height = 3;
    //      optional int32 type = 4;
    //      optional string encryptionKey = 5;
    //    }
    auto pages = page->GetMessageArray(1);

    QStringList imageUrls;
    for (const auto &p : pages)
    {
        if (!p->CheckFieldForType(1, picoproto::FIELD_BYTES))
            continue;

        auto mangapage = p->GetMessage(1);
        std::string xorkey;
        if (mangapage->CheckFieldForType(5, picoproto::FIELD_BYTES))
            xorkey = mangapage->GetString(5);
        auto pageurl = mangapage->GetString(1);

        auto urlencoded = QString::fromUtf8(pageurl.c_str());
        if (xorkey.length() > 0)
            urlencoded += QString("|xor:") + xorkey.c_str();
        imageUrls.append(urlencoded);
    }

    return Ok(imageUrls);
}
