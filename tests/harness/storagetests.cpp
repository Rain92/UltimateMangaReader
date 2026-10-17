// The storage of the app: the cache layout and its files, mangas and chapters by url, the reading progress,
// the favorites and the image links. Against tests/harness/testserver.py.
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>

#include "favoritesmanager.h"
#include "harness.h"
#include "mangacontroller.h"
#include "mangainfo.h"
#include "mangarepository.h"
#include "readingprogress.h"
#include "sizes.h"
#include "sourcelistmodel.h"
#include "ultimatemangareadercore.h"
#include "utils.h"

// A source whose chapters and page lists are set by the test, its info "downloads" go to the test server.
class FakeSource : public MangaSourceBase
{
public:
    QString server;
    MangaChapterCollection nextChapters;
    // the info fails with it, if it is set
    Error nextError;
    QString infoPath = "/final";
    QHash<QString, QList<ImageLink>> pageLists;
    MangaListUpdate nextListUpdate = MangaListUpdate::complete({});
    int listDelay = 0;
    int infoCalls = 0;
    int pageListCalls = 0;
    int resolveCalls = 0;

    FakeSource(NetworkManager *nm, const QString &server) : MangaSourceBase(nm), server(server)
    {
        name = "Fake";
        baseUrl = server;
        headerRules.add("127.0.0.1", "Referer", "https://fake.example/");
    }
    Task<MangaListUpdate> fetchMangaList(UpdateProgressToken *token, MangaList) override
    {
        co_await delay(listDelay);
        if (token->canceled)
            co_return MangaListUpdate::failed({Error::Cancelled, "Download cancelled", 0});
        co_return nextListUpdate;
    }
    Task<Result<MangaDetails>> fetchMangaInfo(QString, MangaChapterCollection) override
    {
        infoCalls++;
        DownloadRequest request;
        request.url = server + infoPath;
        auto job = download(request);
        if (!co_await job)
            co_return Err(job->error);
        if (!nextError.message.isEmpty())
            co_return Err(nextError);

        MangaDetails details;
        details.author = "Author";
        details.chapters = nextChapters;
        co_return details;
    }
    Task<Result<QList<ImageLink>>> fetchPageList(QString chapterUrl) override
    {
        pageListCalls++;
        co_return pageLists.value(chapterUrl);
    }
    Task<Result<ImageLink>> resolveImage(ImageLink page) override
    {
        resolveCalls++;
        co_return page;
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
        core.sources->setSources(harnessSources(core.networkManager, CONF.sourcesDir));
        check(!QFile::exists(cache + "MangaTown/Old-Title"), "old manga directory dropped");
        check(QFile::exists(cache + "mangalists/MangaTown_mangalist.dat"), "manga lists kept");
        check(readFile(cache + "cacheversion").trimmed() == QByteArray::number(CONF.cacheLayoutVersion),
              "layout version written");
        check(QFile::exists(cache + "favorites.dat.old") && !QFile::exists(cache + "favorites.dat") &&
                  core.favoritesManager->count() == 0,
              "old favorites kept aside as favorites.dat.old");
        check(core.settings.mangaOrder == OrderByPopularity && !core.settings.standbyWhenIdle,
              "old settings: defaults");
        auto &list = core.mangaLists.list("MangaTown");
        check(core.sources->source("MangaTown") && core.mangaLists.isLoaded("MangaTown") && list.size == 0,
              "old manga list not read");
        list.append("Title", "/manga/title");
        core.mangaLists.save("MangaTown");
        list = MangaList();
        check(core.mangaLists.reload("MangaTown") && core.mangaLists.list("MangaTown").size == 1,
              "manga list round trip");
        core.settings.standbyWhenIdle = true;
        core.settings.serialize();

        // sources
        int changes = 0;
        QStringList currentSignalled;
        QObject::connect(core.sources, &SourceRegistry::sourcesChanged, [&]() { changes++; });
        QObject::connect(core.sources, &SourceRegistry::currentChanged, [&](const QString &c) { currentSignalled << c; });
        check(core.sources->names().size() == 6 && core.sources->enabledNames() == core.sources->names(),
              "all sources enabled by default");
        core.sources->setCurrent("MangaTown");
        check(core.sources->current() == "MangaTown" && currentSignalled == QStringList{"MangaTown"}, "current source");
        core.sources->setEnabled("MangaTown", false);
        check(!core.sources->isEnabled("MangaTown") && core.sources->enabledNames().size() == 5 && changes == 1 &&
                  !core.settings.enabledMangaSources.value("MangaTown", true),
              "disabled source, in the settings");
        check(core.sources->current().isEmpty() && currentSignalled.last().isEmpty(),
              "disabled current source is unset");
        core.sources->setEnabled("MangaTown", false);
        check(changes == 1, "no change, no signal");
        check(!core.sources->isEnabled("Unknown") && !core.sources->source("Unknown"), "unknown source isn't enabled");

        // the dialogs: all sources to enable, the enabled ones to select
        SourceListModel all(core.sources, SourceListModel::EnableSources);
        SourceListModel selection(core.sources, SourceListModel::SelectSources);
        int resets = 0;
        QObject::connect(&selection, &QAbstractItemModel::modelReset, [&]() { resets++; });
        // (the order of the index)
        int town = int(core.sources->names().indexOf("MangaTown"));
        check(all.rowCount() == 6 && !all.isChecked("MangaTown") && all.checkedNames().size() == 5 &&
                  all.index(town).data(Qt::CheckStateRole).toInt() == Qt::Unchecked &&
                  all.index(town).data().toString() == "MangaTown",
              "all sources, checked are the enabled ones");
        check(selection.rowCount() == 5 && selection.checkedNames() == core.sources->enabledNames(),
              "the enabled sources, all selected");
        all.setData(all.index(town), Qt::Checked, Qt::CheckStateRole);
        check(core.sources->isEnabled("MangaTown") && changes == 2 && resets == 1 && selection.rowCount() == 6 &&
                  selection.isChecked("MangaTown"),
              "a check enables the source, the other list follows");
        selection.setChecked("MangaDex", false);
        check(!selection.checkedNames().contains("MangaDex") && selection.checkedNames().size() == 5 &&
                  core.sources->isEnabled("MangaDex"),
              "a selection doesn't change the sources");
        selection.selectAll();
        check(selection.checkedNames().size() == 6, "select all");

        // lists: only the loaded ones are sorted and signalled
        QStringList changed;
        QObject::connect(core.mangaListUpdater, &MangaListUpdater::mangaListChanged,
                         [&](const QString &m) { changed << m; });
        core.mangaListUpdater->sortMangaLists();
        check(changed == QStringList{"MangaTown"}, "sorting signals the loaded lists");
    }
    touch(cache + "MangaTown/0123456789abcdef/keep");
    {
        UltimateMangaReaderCore core;
        core.sources->setSources(harnessSources(core.networkManager, CONF.sourcesDir));
        check(QFile::exists(cache + "MangaTown/0123456789abcdef/keep"), "cache of the current layout kept");
        check(core.settings.standbyWhenIdle, "settings round trip");
    }

    // --- mangas by url
    NetworkManager nm;
    nm.setDownloadSettings(QSize(1404, 1872), nullptr);
    Settings settings;
    nm.setDownloadSettings(QSize(1404, 1872), &settings);
    auto fake = QSharedPointer<FakeSource>::create(&nm, server);
    SourceRegistry sources(&settings, {fake});
    MangaRepository repository(&nm, &sources);
    const QString url = "https://fake.example/manga/test";
    const QString url2 = "https://fake.example/manga/test-2";

    fake->nextChapters = chapterList({"A", "B", "C"});
    auto infoR = repository.loadInfo("Fake", url, "Same Title").wait();
    check(infoR.has_value(), "load info");
    auto info = infoR.value();
    check(info->url == url && info->title == "Same Title" && info->chapters.size() == 3 && info->author == "Author",
          "info has url, title and the details of the source");
    check(info->directory() == CONF.mangasourcedir("Fake") + urlHash(url, 16) + "/" &&
              QFile::exists(info->directory() + "mangainfo.dat"),
          "info saved in the directory of its url (also without a cover)");

    fake->nextChapters = chapterList({"D"});
    auto info2 = repository.loadInfo("Fake", url2, "Same Title").wait().value();
    check(info2 != info && info2->chapters.size() == 1 && info->chapters.size() == 3 &&
              info2->directory() != info->directory(),
          "same title, other url: another manga");
    check(repository.loadInfo("Fake", url, "Renamed", false).wait().value() == info, "same url: the live info");

    // --- page lists
    auto B = info->chapters[1].chapterUrl;
    fake->pageLists[B] = {ImageLink::fromUrl(server + "/image/100x150"), ImageLink::fromUrl(server + "/image/100x151")};
    check(repository.loadPageList(info, 1).wait().has_value() && info->chapters[1].pages.size() == 2 &&
              info->chapters[1].pagesLoaded && fake->pageListCalls == 1,
          "page list loaded");
    check(repository.loadPageList(info, 1).wait().has_value() && fake->pageListCalls == 1,
          "loaded page list not loaded again");
    info->serialize();
    {
        QWeakPointer<MangaInfo> weak = info;
        auto path = info->directory() + "mangainfo.dat";
        auto copy = MangaInfo::deserialize(path);
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
        repository.updateInfo(info, false);
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
    check(repository.loadPageList(info, 0).wait().has_value() && !QFile::exists(info->pagePath(X, 0)) &&
              !QFile::exists(info->pagePath(X, 5)) && QFile::exists(info->pagePath(B, 0)),
          "old files of a chapter whose page list is loaded removed");

    // --- the images of the pages
    {
        auto image = repository.pageImage(info, 0, 0).wait();
        check(image && image->url == server + "/image/100x150" && fake->resolveCalls == 1 &&
                  info->chapters[0].pages[0].image.url == image->url,
              "page image resolved and kept");
        check(repository.pageImage(info, 0, 0).wait() && fake->resolveCalls == 1, "kept page image not resolved again");

        info->chapters[0].pages[0].image.expires = QDateTime::currentSecsSinceEpoch() - 1;
        check(repository.pageImage(info, 0, 0).wait() && fake->resolveCalls == 2, "expired page image resolved again");

        info->serialize();
        auto copy = MangaInfo::deserialize(info->directory() + "mangainfo.dat");
        check(copy && copy->chapters[0].pages[0].image.isEmpty() && !copy->chapters[0].pages[0].link.isEmpty(),
              "page images aren't read from the file, the page lists are");

        auto broken = ImageLink::fromUrl(server + "/status/404");
        info->chapters[0].pages[0].image = broken;
        auto job = repository.downloadImage("Fake", broken, info->directory() + "test/broken.jpg");
        job->await(5000);
        check(job->hasFailed() && info->chapters[0].pages[0].image.isEmpty(),
              "a page image that couldn't be downloaded is forgotten");

        // the retries of the source (e.g. longer for a CDN that answers 429 while it loads an image)
        fake->imageRetries = {5, 50, 100};
        auto retried = repository.downloadImage("Fake", ImageLink::fromUrl(server + "/status/503"),
                                                info->directory() + "test/retried.jpg");
        retried->await(5000);
        auto descriptor = repository.imageDownload("Fake", broken, info->directory() + "test/other.jpg");
        check(retried->hasFailed() && retried->request.maxRetries == 5 && retried->request.retryDelay == 50 &&
                  retried->request.maxRetryDelay == 100 && descriptor.retries.maxRetries == 5,
              "the image downloads have the retries of their source");
        fake->imageRetries = {};
    }

    // --- updates of the info
    {
        fake->nextChapters = chapterList({"X", "B", "Y"});
        int calls = fake->infoCalls;
        auto first = repository.updateInfo(info);
        auto second = repository.updateInfo(info);
        check(first.wait() && second.wait() && fake->infoCalls == calls + 1, "an update that runs is joined");

        QList<Error> signalled;
        auto c = QObject::connect(&repository, &MangaRepository::infoUpdateFailed,
                                  [&](QSharedPointer<MangaInfo> i, const Error &e)
                                  {
                                      if (i == info)
                                          signalled << e;
                                  });
        fake->nextError = {Error::Parse, "broken page", 0};
        auto failed = repository.updateInfo(info).wait();
        check(!failed && failed.error().code == Error::Parse && signalled.size() == 1 &&
                  signalled[0].message == "broken page" && info->chapters.size() == 3,
              "a failed update is signalled, the info stays");
        fake->nextError = {};

        fake->nextChapters = chapterList({"X"});
        auto shrunk = repository.updateInfo(info).wait();
        check(!shrunk && shrunk.error().code == Error::Parse && signalled.size() == 2 && info->chapters.size() == 3,
              "a chapter list that shrank a lot isn't taken");
        QObject::disconnect(c);

        auto gone = repository.loadInfo("Gone", url, "Title").wait();
        check(!gone && gone.error().code == Error::NotFound, "the manga of a source that isn't there: an error");
    }

    // --- the reader follows its chapter
    {
        ReadingProgress(MangaIndex(1, 1), B, 3, 2).serialize("Fake", url);
        MangaController controller(&nm, &repository);
        controller.setCurrentManga(info);
        check(controller.currentIndex.chapter == 1 && controller.currentIndex.page == 1, "reader opens at the progress");
        check(update({"B", "X", "Y"}), "update 5 done");
        check(controller.currentIndex.chapter == 0 && controller.currentIndex.page == 1 &&
                  controller.currentIndex.currentChapter().chapterUrl == B,
              "reader moved with its chapter");
        ReadingProgress p("Fake", url);
        check(p.index.chapter == 0 && p.chapterUrl == B, "reader saved the progress");
    }

    // --- the progress of a page turn is written a moment later, it is read from memory until then
    {
        auto onDisk = [&]
        {
            QFile f(CONF.mangainfodir("Fake", url) + "progress.dat");
            return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        };
        ReadingProgress before("Fake", url);
        ReadingProgress(MangaIndex(1, 0), B, 3, 2).serialize("Fake", url);
        auto written = onDisk();
        ReadingProgress(MangaIndex(1, 5), B, 3, 9).serializeLater("Fake", url);
        check(onDisk() == written && ReadingProgress("Fake", url).index.page == 5,
              "a progress kept for later is read before it is written");
        ReadingProgress::flushPending();
        check(onDisk() != written && ReadingProgress("Fake", url).index.page == 5, "flushPending() writes it");
        ReadingProgress(MangaIndex(1, 6), B, 3, 9).serializeLater("Fake", url);
        ReadingProgress(MangaIndex(1, 7), B, 3, 9).serialize("Fake", url);
        ReadingProgress::flushPending();
        check(ReadingProgress("Fake", url).index.page == 7, "a write drops the progress kept for later");
        before.serialize("Fake", url);
    }

    // --- favorites by url
    {
        FavoritesManager favorites(&sources, &repository, &nm);
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
        FavoritesManager favorites(&sources, &repository, &nm);
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

        // the favorites of a disabled source have no infos, they come back when it's enabled again
        events.clear();
        sources.setEnabled("Fake", false);
        check(events == QStringList({"chg 0", "chg 1"}) && !favorites.info(0) && !favorites.info(1) &&
                  favorites.count() == 2,
              "a disabled source: its favorites stay, without infos");
        events.clear();
        sources.setEnabled("Fake", true);
        check(events == QStringList({"chg 0", "chg 1"}) && favorites.info(0) == info && favorites.info(1) == info2,
              "enabled again: the live infos");

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
        auto job = repository.downloadImage("Fake", image, path);

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
    DownloadRequest lastHeaders;
    lastHeaders.url = server + "/lastheaders";
    auto headersJob = nm.download(lastHeaders);
    headersJob->await();
    check(headersJob->text().contains("X-Test: abc") && headersJob->text().contains("Referer: https://fake.example/"),
          "image request has the link headers and the source headers");

    {
        auto path = info->directory() + "test/ready.jpg";
        QStringList events;
        auto job = repository.downloadImage("Fake", ImageLink::fromUrl(server + "/image/100x153"), path);
        auto handedOut = QObject::connect(&nm, &NetworkManager::downloadedImage,
                                          [&](const QString &p, QSharedPointer<QImage> img)
                                          {
                                              if (p == path)
                                                  events << QString("image %1 %2")
                                                                .arg(img && !img->isNull())
                                                                .arg(job->isCompleted);
                                          });
        QObject::connect(job.get(), &DownloadJobBase::completed,
                         [&]() { events << QString("completed %1").arg(QFile::exists(path)); });
        job->await(10000);
        QObject::disconnect(handedOut);
        check(events == QStringList{"image 1 0", "completed 1"}, "the processed page is handed out before it is saved");
    }

    out << (failures == 0 ? "all ok" : QString::number(failures) + " FAILED") << "\n";
    // --- manga list updates
    {
        MangaListStore lists;
        MangaListUpdater updater(&sources, &lists, &settings, nullptr);
        QStringList changed;
        QObject::connect(&updater, &MangaListUpdater::mangaListChanged, [&](const QString &s) { changed << s; });

        // like the dialog: an update ends with finished or an error, a cancelled one with nothing
        auto run = [&](bool cancel)
        {
            QSharedPointer<UpdateProgressToken> token(new UpdateProgressToken());
            token->sourcesProgress["Fake"] = 0;
            QString outcome;
            QObject::connect(token.get(), &UpdateProgressToken::updateFinished,
                             [&](const QString &m) { outcome = "finished:" + m; });
            QObject::connect(token.get(), &UpdateProgressToken::updateError,
                             [&](const QString &e) { outcome = "error:" + e; });
            updater.updateMangaLists(token);
            token->canceled = cancel;
            QElapsedTimer t;
            t.start();
            while (outcome.isEmpty() && t.elapsed() < (cancel ? 500 : 3000))
                QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 20);
            return outcome;
        };
        auto titles = [](int count)
        {
            MangaList list;
            for (int i = 0; i < count; i++)
                list.append(QString("Title %1").arg(i), QString("/manga/%1").arg(i));
            return list;
        };

        fake->nextListUpdate = MangaListUpdate::complete(titles(10));
        auto outcome = run(false);
        lists.reload("Fake");
        check(outcome == "finished:" && lists.list("Fake").size == 10 && changed == QStringList{"Fake"},
              "list updated, saved and signalled");

        fake->nextListUpdate = MangaListUpdate::partial(titles(9), {Error::Http, "No ranking.", 404});
        outcome = run(false);
        check(outcome == "finished:Fake: No ranking." && lists.list("Fake").size == 9,
              "a partial update is taken, with a message");

        fake->nextListUpdate = MangaListUpdate::failed({Error::Network, "Site down.", 0});
        outcome = run(false);
        check(outcome == "error:Site down." && lists.list("Fake").size == 9, "a failed update keeps the list");

        fake->nextListUpdate = MangaListUpdate::complete(titles(3));
        outcome = run(false);
        check(outcome.startsWith("error:Fake: the update found only 3 of 9 titles") && lists.list("Fake").size == 9,
              "a list that shrank a lot isn't taken");

        changed.clear();
        fake->listDelay = 200;
        fake->nextListUpdate = MangaListUpdate::complete(titles(10));
        outcome = run(true);
        check(outcome.isEmpty() && changed.isEmpty() && lists.list("Fake").size == 9,
              "a cancelled update ends quietly, the list stays");
        fake->listDelay = 0;
        check(run(false) == "finished:" && lists.list("Fake").size == 10, "the next update after a cancel");

        changed.clear();
        sources.setEnabled("Fake", false);
        check(run(false) == "finished:" && changed.isEmpty(), "a disabled source isn't updated");
        sources.setEnabled("Fake", true);
    }

    return failures == 0 ? 0 : 1;
}
