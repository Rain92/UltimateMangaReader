#pragma once
#include <QApplication>
#include <QDateTime>
#include <QImage>
#include <QRegularExpression>
#include <QTextDocument>

#include "downloadqueue.h"
#include "imagelink.h"
#include "mangachaptercollection.h"
#include "mangalist.h"
#include "networkmanager.h"
#include "sizes.h"
#include "staticsettings.h"
#include "thirdparty/result.h"
#include "updateprogresstoken.h"

class MangaInfo;

class AbstractMangaSource
{
public:
    QString name;

    QString baseUrl;

    MangaList mangaList;

    AbstractMangaSource(NetworkManager *networkManager);
    virtual ~AbstractMangaSource() = default;

    virtual bool updateMangaList(UpdateProgressToken *token) = 0;

    virtual Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> mangainfo) = 0;

    virtual Result<QSharedPointer<MangaInfo>, QString> getMangaInfo(const QString &mangaUrl,
                                                                    const QString &mangaTitle);
    // The url the info of a manga is downloaded from (by default its url).
    virtual QString mangaInfoRequestUrl(const QString &mangaUrl) const;

    // The pages of a chapter: their images, or pages of the site that getImageLink() resolves.
    virtual Result<QList<ImageLink>, QString> getPageList(const QString &chapterUrl) = 0;
    // The image of a page of the page list (also again, when the one it resolved to before expired).
    virtual Result<ImageLink, QString> getImageLink(const ImageLink &page);

    Result<QSharedPointer<MangaInfo>, QString> loadMangaInfo(const QString &mangaUrl,
                                                             const QString &mangatitle, bool update = true);
    // The cached info, without downloading anything. If there is none it only has its title and url.
    QSharedPointer<MangaInfo> loadCachedMangaInfo(const QString &mangaUrl, const QString &mangaTitle);

    bool serializeMangaList();
    bool deserializeMangaList();
    // Loads the list from its file the first time it is needed: the lists of all sources have tens of MB
    // (seconds to read on the device, more in memory) and most sessions don't need them.
    void loadMangaList();
    bool mangaListLoaded = false;
    // the order of the list setting: a list that was saved in another one is sorted when it's loaded
    MangaOrderMethod listOrder = OrderByPopularity;

    // The download of an image to path (a page of a manga, see MangaInfo::pagePath()), with the headers of the
    // source.
    FileDownloadDescriptor imageDownload(const ImageLink &image, const QString &path) const;
    QSharedPointer<DownloadFileJob> downloadImage(const ImageLink &image, const QString &path);

    // The headers of the source's requests to url: the header rules for its host.
    HttpHeaders requestHeaders(const QString &url) const;

    QString htmlToPlainText(const QString &str);

    virtual void updateMangaInfoAsync(QSharedPointer<MangaInfo> mangainfo, bool updateCover = true);
    void downloadCoverAsync(QSharedPointer<MangaInfo> mangainfo, bool updateCover = true);
    // Another url to try when the cover can't be downloaded (e.g. the same file on another host of the cdn).
    virtual QString coverFallbackUrl(const QString &coverUrl)
    {
        Q_UNUSED(coverUrl)
        return {};
    }
    Result<void, QString> updatePageList(QSharedPointer<MangaInfo> info, int chapter);

protected:
    int mangaInfoTimeout;

    NetworkManager *networkManager;
    QTextDocument htmlConverter;

    // e.g. the Referer the image hosts want
    HeaderRules headerRules;

    // The downloads of the source, with its headers.
    QSharedPointer<DownloadStringJob> downloadAsString(const QString &url, int timeout = 6000);
    QSharedPointer<DownloadBufferJob> downloadToBuffer(const QString &url, int timeout = 6000);
    QSharedPointer<DownloadFileJob> downloadAsFile(const QString &url, const QString &localPath);
    QList<QSize> probeImageSizes(const QStringList &urls, int timeout = 10000, QList<int> *timedOut = nullptr);
    // for a DownloadQueue
    std::function<HttpHeaders(const QString &)> requestHeadersFunction() const;

    // Merges the chapters an update loaded into the info (removes the downloaded pages of the chapters that are
    // gone).
    Result<void, QString> mergeChapters(QSharedPointer<MangaInfo> info, const MangaChapterCollection &newChapters);
    void removeChapterPages(QSharedPointer<MangaInfo> info, const QString &chapterUrl);
    void removePagesOfRemovedChapters(QSharedPointer<MangaInfo> info);

    // The infos in use, by url (like their files): every part of the app gets the same object for a manga.
    // Two copies would both apply a chapter update to the pages and the reading progress.
    QHash<QString, QWeakPointer<MangaInfo>> liveInfos;
    QSharedPointer<MangaInfo> liveInfo(const QString &mangaUrl);
    void registerInfo(const QSharedPointer<MangaInfo> &info);

    QString defaultCoverPath(QSharedPointer<MangaInfo> mangainfo);
    void fillMangaInfo(QSharedPointer<MangaInfo> info, const QString &buffer,
                       const QRegularExpression &authorrx, const QRegularExpression &artistrx,
                       const QRegularExpression &statusrx, const QRegularExpression &yearrx,
                       const QRegularExpression &genresrx, const QRegularExpression &summaryrx,
                       const QRegularExpression &coverrx);
};
