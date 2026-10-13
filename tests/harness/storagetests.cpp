// The storage of the app: the cache layout and its files, mangas and chapters by url, the reading progress,
// the favorites and the image links. Against tests/harness/testserver.py.
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>

#include "favoritesmanager.h"
#include "harness.h"
#include "mangacontroller.h"
#include "mangainfo.h"
#include "readingprogress.h"
#include "ultimatemangareadercore.h"
#include "utils.h"

// A source whose chapters and page lists are set by the test, its info "downloads" go to the test server.
class FakeSource : public AbstractMangaSource
{
public:
    QString server;
    MangaChapterCollection nextChapters;
    QHash<QString, QList<ImageLink>> pageLists;
    int pageListCalls = 0;

    FakeSource(NetworkManager *nm, const QString &server) : AbstractMangaSource(nm), server(server)
    {
        name = "Fake";
        baseUrl = server;
        headerRules.add("127.0.0.1", "Referer", "https://fake.example/");
    }
    bool updateMangaList(UpdateProgressToken *) override { return true; }
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob>,
                                                                          QSharedPointer<MangaInfo>) override
    {
        return Ok(nextChapters);
    }
    QString mangaInfoRequestUrl(const QString &) const override { return server + "/final"; }
    Result<QList<ImageLink>, QString> getPageList(const QString &chapterUrl) override
    {
        pageListCalls++;
        return Ok(pageLists.value(chapterUrl));
    }
};

static MangaChapterCollection chapterList(const QStringList &names)
{
    MangaChapterCollection chapters;
    for (const auto &n : names)
        chapters.append(MangaChapter("Chapter " + n, "https://fake.example/chapter/" + n));
    return chapters;
}

static void touch(const QString &path)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    (void)f.open(QIODevice::WriteOnly);
    f.write("x");
}

int storageTests(const QString &server)
{
    int failures = 0;
    auto check = [&](bool ok, const QString &what)
    {
        out << (ok ? "ok   " : "FAIL ") << what << "\n";
        out.flush();
        failures += !ok;
    };
    auto readFile = [](const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    };

    // --- the cache of the old layout and the old file formats
    QString cache = CONF.cacheDir;
    touch(cache + "MangaTown/Old-Title/images/0_0.jpg");
    touch(cache + "MangaTown/Old-Title/mangainfo.dat");
    {
        QFile f(cache + "mangalists/MangaTown_mangalist.dat");
        QDir().mkpath(cache + "mangalists");
        (void)f.open(QIODevice::WriteOnly);
        QDataStream o(&f);
        o.setVersion(QDataStream::Qt_5_15);
        MangaList list;
        list.append("Title", "/manga/title");
        o << list;
    }
    {
        QFile f(cache + "favorites.dat");
        (void)f.open(QIODevice::WriteOnly);
        QDataStream o(&f);
        o.setVersion(QDataStream::Qt_5_15);
        o << QList<Favorite>{Favorite("MangaTown", "Title", "https://www.mangatown.com/manga/title/")};
    }
    {
        QFile f(cache + "settings.dat");
        (void)f.open(QIODevice::WriteOnly);
        f.write(QByteArray(40, 1));
    }

    {
        UltimateMangaReaderCore core;
        check(!QFile::exists(cache + "MangaTown/Old-Title"), "old manga directory dropped");
        check(QFile::exists(cache + "mangalists/MangaTown_mangalist.dat"), "manga lists kept");
        check(readFile(cache + "cacheversion").trimmed() == QByteArray::number(CONF.cacheLayoutVersion),
              "layout version written");
        check(QFile::exists(cache + "favorites.dat.old") && !QFile::exists(cache + "favorites.dat") &&
                  core.favoritesManager->count() == 0,
              "old favorites kept aside as favorites.dat.old");
        check(core.settings.mangaOrder == OrderByPopularity && !core.settings.standbyWhenIdle,
              "old settings: defaults");
        auto ms = findSource(core.sources->all(), "MangaTown");
        check(ms->name == "MangaTown" && !ms->deserializeMangaList() && ms->mangaList.size == 0,
              "old manga list not read");
        ms->mangaList.append("Title", "/manga/title");
        ms->serializeMangaList();
        ms->mangaList = MangaList();
        check(ms->deserializeMangaList() && ms->mangaList.size == 1, "manga list round trip");
        core.settings.standbyWhenIdle = true;
        core.settings.serialize();

        // sources
        QList<AbstractMangaSource *> activeSignalled;
        AbstractMangaSource *currentSignalled = ms;
        QObject::connect(core.sources, &SourceRegistry::activeSourcesChanged, [&](const QList<AbstractMangaSource *> &l) { activeSignalled = l; });
        QObject::connect(core.sources, &SourceRegistry::currentSourceChanged, [&](AbstractMangaSource *c) { currentSignalled = c; });
        check(core.sources->active().size() == 6 && core.settings.enabledMangaSources.size() == 6, "all sources enabled by default");
        core.sources->setCurrent(ms);
        check(core.sources->current() == ms && currentSignalled == ms, "current source");
        core.settings.enabledMangaSources["MangaTown"] = false;
        core.updateActiveSources();
        check(core.sources->active().size() == 5 && !core.sources->active().contains("MangaTown") && activeSignalled.size() == 5,
              "disabled source not active");
        check(core.sources->current() == nullptr && currentSignalled == nullptr, "disabled current source is unset");
        core.settings.enabledMangaSources["MangaTown"] = true;
        core.updateActiveSources();

        // lists: only the loaded ones are sorted and signalled
        QList<AbstractMangaSource *> changed;
        QObject::connect(core.mangaListUpdater, &MangaListUpdater::mangaListChanged, [&](AbstractMangaSource *m) { changed << m; });
        core.mangaListUpdater->sortMangaLists();
        check(changed == QList<AbstractMangaSource *>{ms}, "sorting signals the loaded lists");
    }
    touch(cache + "MangaTown/0123456789abcdef/keep");
    {
        UltimateMangaReaderCore core;
        check(QFile::exists(cache + "MangaTown/0123456789abcdef/keep"), "cache of the current layout kept");
        check(core.settings.standbyWhenIdle, "settings round trip");
    }

    // --- mangas by url
    NetworkManager nm;
    nm.setDownloadSettings(QSize(1404, 1872), nullptr);
    Settings settings;
    nm.setDownloadSettings(QSize(1404, 1872), &settings);
    auto fake = QSharedPointer<FakeSource>::create(&nm, server);
    QMap<QString, AbstractMangaSource *> sources{{"Fake", fake.get()}};
    const QString url = "https://fake.example/manga/test";
    const QString url2 = "https://fake.example/manga/test-2";

    fake->nextChapters = chapterList({"A", "B", "C"});
    auto infoR = fake->loadMangaInfo(url, "Same Title");
    check(infoR.isOk(), "load info");
    auto info = infoR.unwrap();
    check(info->url == url && info->title == "Same Title" && info->chapters.size() == 3, "info has url and title");
    check(info->directory() == CONF.mangasourcedir("Fake") + urlHash(url, 16) + "/" &&
              QFile::exists(info->directory() + "mangainfo.dat"),
          "info saved in the directory of its url (also without a cover)");

    fake->nextChapters = chapterList({"D"});
    auto info2 = fake->loadMangaInfo(url2, "Same Title").unwrap();
    check(info2 != info && info2->chapters.size() == 1 && info->chapters.size() == 3 &&
              info2->directory() != info->directory(),
          "same title, other url: another manga");
    check(fake->loadMangaInfo(url, "Renamed", false).unwrap() == info, "same url: the live info");

    // --- page lists
    auto B = info->chapters[1].chapterUrl;
    fake->pageLists[B] = {ImageLink::fromUrl(server + "/image/100x150"), ImageLink::fromUrl(server + "/image/100x151")};
    check(fake->updatePageList(info, 1).isOk() && info->chapters[1].pages.size() == 2 &&
              info->chapters[1].pagesLoaded && fake->pageListCalls == 1,
          "page list loaded");
    check(fake->updatePageList(info, 1).isOk() && fake->pageListCalls == 1, "loaded page list not loaded again");
    info->serialize();
    {
        QWeakPointer<MangaInfo> weak = info;
        auto path = info->directory() + "mangainfo.dat";
        auto copy = MangaInfo::deserialize(fake.get(), path);
        check(copy && copy->url == url && copy->chapters.size() == 3 && copy->chapters[1].pages.size() == 2 &&
                  copy->chapters[1].pages[0].link.url == server + "/image/100x150",
              "info round trip");
    }

    // --- chapter updates
    auto A = info->chapters[0].chapterUrl, C = info->chapters[2].chapterUrl;
    touch(info->pagePath(A, 0));
    touch(info->pagePath(B, 0));
    touch(info->pagePath(B, 1));
    touch(info->pagePath(C, 0));
    ReadingProgress(MangaIndex(1, 1), B, 3, 2).serialize("Fake", url);

    int moved = 0;
    bool newChapters = false;
    QObject::connect(info.get(), &MangaInfo::chaptersMoved, [&]() { moved++; });
    QObject::connect(info.get(), &MangaInfo::updatedSignal, [&](bool n) { newChapters = n; });
    auto update = [&](const QStringList &names)
    {
        fake->nextChapters = chapterList(names);
        bool done = false;
        auto c = QObject::connect(info.get(), &MangaInfo::updatedSignal, [&]() { done = true; });
        fake->updateMangaInfoAsync(info, false);
        QElapsedTimer t;
        t.start();
        while (!done && t.elapsed() < 5000)
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 100);
        QObject::disconnect(c);
        return done;
    };

    check(update({"X", "C", "B"}), "update 1 done");
    check(info->chapters.size() == 3 && info->chapters[2].chapterUrl == B && info->chapters[2].pagesLoaded &&
              info->chapters[2].pages.size() == 2 && !info->chapters[0].pagesLoaded,
          "pages stay with their chapter");
    check(!QFile::exists(info->pagePath(A, 0)) && QFile::exists(info->pagePath(B, 0)) &&
              QFile::exists(info->pagePath(B, 1)) && QFile::exists(info->pagePath(C, 0)),
          "pages of the removed chapter deleted, the others kept");
    check(moved == 1 && newChapters && info->updated, "moved and new chapters signalled");
    {
        ReadingProgress p("Fake", url);
        check(p.index.chapter == 2 && p.index.page == 1 && p.chapterUrl == B, "reading progress follows its chapter");
    }

    moved = 0;
    check(update({"X", "C", "B", "Y"}), "update 2 done");
    check(moved == 0 && newChapters, "appended chapter: nothing moved");

    check(update({"X", "C", "B", "Y"}), "update 3 done");
    check(moved == 0 && !newChapters, "same chapters: nothing new");

    ReadingProgress(MangaIndex(1, 3), C, 4, 5).serialize("Fake", url);
    check(update({"X", "B", "Y"}), "update 4 done");
    {
        ReadingProgress p("Fake", url);
        check(p.index.chapter == 1 && p.index.page == 0 && p.chapterUrl == B,
              "progress of a removed chapter: start of the chapter at its place");
        check(!QFile::exists(info->pagePath(C, 0)), "pages of removed chapter C deleted");
    }

    // a page list loaded again replaces the files of the chapter
    auto X = info->chapters[0].chapterUrl;
    touch(info->pagePath(X, 0));
    touch(info->pagePath(X, 5));
    fake->pageLists[X] = {ImageLink::fromUrl(server + "/image/100x150")};
    check(fake->updatePageList(info, 0).isOk() && !QFile::exists(info->pagePath(X, 0)) &&
              !QFile::exists(info->pagePath(X, 5)) && QFile::exists(info->pagePath(B, 0)),
          "old files of a chapter whose page list is loaded removed");

    // --- the reader follows its chapter
    {
        ReadingProgress(MangaIndex(1, 1), B, 3, 2).serialize("Fake", url);
        MangaController controller(&nm);
        controller.setCurrentManga(info);
        check(controller.currentIndex.chapter == 1 && controller.currentIndex.page == 1, "reader opens at the progress");
        check(update({"B", "X", "Y"}), "update 5 done");
        check(controller.currentIndex.chapter == 0 && controller.currentIndex.page == 1 &&
                  controller.currentIndex.currentChapter().chapterUrl == B,
              "reader moved with its chapter");
        ReadingProgress p("Fake", url);
        check(p.index.chapter == 0 && p.chapterUrl == B, "reader saved the progress");
    }

    // --- favorites by url
    {
        FavoritesManager favorites(sources, &nm);
        QStringList events;
        QObject::connect(&favorites, &FavoritesManager::favoriteInserted, [&](int i) { events << QString("ins %1").arg(i); });
        QObject::connect(&favorites, &FavoritesManager::favoriteRemoved, [&](int i) { events << QString("rem %1").arg(i); });
        favorites.toggleFavorite(info);
        favorites.toggleFavorite(info2);
        check(favorites.isFavorite(info) && favorites.isFavorite(info2) && favorites.count() == 2,
              "two favorites of the same title");
        check(events == QStringList({"ins 0", "ins 0"}), "insertions signalled");
        check(favorites.progress(1).index.chapter == 0 && favorites.progress(1).chapterUrl == B,
              "a new favorite has its reading progress");
    }
    {
        FavoritesManager favorites(sources, &nm);
        check(favorites.count() == 2 && favorites.favorite(1).mangaUrl == url &&
                  favorites.favorite(0).mangaUrl == url2 && !favorites.info(0),
              "favorites round trip, infos not loaded yet");
        int resets = 0;
        QObject::connect(&favorites, &FavoritesManager::favoritesReset, [&]() { resets++; });
        favorites.loadInfos();
        check(resets == 1 && favorites.info(1) == info && favorites.info(0) == info2, "favorites get the live infos");
        check(favorites.progress(1).chapterUrl == B, "progress loaded with the infos");

        QStringList events;
        QObject::connect(&favorites, &FavoritesManager::favoriteChanged, [&](int i) { events << QString("chg %1").arg(i); });
        QObject::connect(&favorites, &FavoritesManager::favoriteMoved, [&](int a, int b) { events << QString("mov %1 %2").arg(a).arg(b); });
        QObject::connect(&favorites, &FavoritesManager::favoriteRemoved, [&](int i) { events << QString("rem %1").arg(i); });

        ReadingProgress p(MangaIndex(0, 7), B, 3, 9);
        favorites.setProgress(info.get(), p);
        check(events == QStringList({"chg 1"}) && favorites.progress(1).index.page == 7, "reader progress taken over");

        // new chapters move the favorite to the front, moved ones relocate its progress
        events.clear();
        ReadingProgress(MangaIndex(0, 7), B, 3, 9).serialize("Fake", url);
        check(update({"Z", "B", "X", "Y"}), "update 6 done");
        check(events == QStringList({"mov 1 0", "chg 0"}) && favorites.info(0) == info &&
                  favorites.progress(0).index.chapter == 1 && favorites.progress(0).index.page == 7,
              "favorite with new chapters moved to the front, progress located again");

        events.clear();
        favorites.toggleFavorite(info2);
        check(events == QStringList({"rem 1"}) && favorites.count() == 1 && favorites.isFavorite(info) &&
                  !favorites.isFavorite(info2),
              "remove the favorite of one of them");

        // thumbnails
        QImage cover(300, 450, QImage::Format_Grayscale8);
        cover.fill(128);
        info->coverPath = info->directory() + "cover.jpg";
        cover.save(info->coverPath);
        QString thumbKey;
        QSize thumbSize;
        QObject::connect(&favorites, &FavoritesManager::thumbnailLoaded, [&](const QString &k, const QImage &img) { thumbKey = k; thumbSize = img.size(); });
        favorites.loadThumbnails({0}, 2.0);
        QElapsedTimer t;
        t.start();
        while (thumbKey.isEmpty() && t.elapsed() < 5000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        check(thumbKey == "Fake/" + url && thumbSize.height() == qRound(SIZES.favoriteCoverSize * 2.0) &&
                  QFile::exists(info->coverThumbnailPath()),
              "thumbnail made from the cover and saved");
    }

    {
        QFile f(CONF.mangainfodir("Fake", url2) + "progress.dat");
        (void)f.open(QIODevice::WriteOnly);
        QDataStream o(&f);
        o.setVersion(QDataStream::Qt_5_15);
        o << MangaIndex(3, 4) << 5 << 6;
        f.close();
        ReadingProgress p;
        check(!p.deserialize("Fake", url2) && p.index.chapter == 0, "old reading progress not read");
    }

    // --- image links
    auto download = [&](const ImageLink &image, const QString &name)
    {
        auto path = info->directory() + "test/" + name + ".jpg";
        auto job = fake->downloadImage(image, path);
        job->await(10000);
        return QImage(path).size();
    };
    auto plain = download(ImageLink::fromUrl(server + "/image/100x150"), "plain");
    check(!plain.isEmpty(), "plain image");

    ImageLink encrypted = ImageLink::fromUrl(server + "/xorimage/100x150/a1b2c3");
    encrypted.xorKey = QByteArray::fromHex("a1b2c3");
    check(download(encrypted, "xor") == plain, "xor encrypted image");
    encrypted.xorKey.clear();
    check(download(encrypted, "xor-nokey").isEmpty(), "xor encrypted image without key fails");

    ImageLink joined = ImageLink::fromUrl(server + "/image/100x150");
    joined.appendUrl = server + "/image/100x50";
    auto joinedSize = download(joined, "joined");
    check(!joinedSize.isEmpty() && qAbs(double(joinedSize.width()) / joinedSize.height() - 0.5) < 0.01,
          "appended image stitched below");

    ImageLink withHeaders = ImageLink::fromUrl(server + "/image/100x152");
    withHeaders.headers.append(HttpHeader{"X-Test", "abc"});
    download(withHeaders, "headers");
    auto headersJob = nm.downloadAsString(server + "/lastheaders");
    headersJob->await(5000);
    check(headersJob->bufferStr.contains("X-Test: abc") && headersJob->bufferStr.contains("Referer: https://fake.example/"),
          "image request has the link headers and the source headers");

    out << (failures == 0 ? "all ok" : QString::number(failures) + " FAILED") << "\n";
    return failures == 0 ? 0 : 1;
}
