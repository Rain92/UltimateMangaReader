#ifndef ABSTRACTMANGASOURCE_H
#define ABSTRACTMANGASOURCE_H
#include <QDateTime>
#include <QImage>
#include <QRegularExpression>
#include <QTextDocument>

#include "downloadimagedescriptor.h"
#include "networkmanager.h"
#include "downloadqueue.h"
#include "mangachapter.h"
#include "mangalist.h"
#include "result.h"
#include "sizes.h"
#include "staticsettings.h"
#include "updateprogresstoken.h"

class MangaInfo;

class AbstractMangaSource
{
public:
    QString name;

    QString baseurl;

    MangaList mangaList;

    AbstractMangaSource(NetworkManager *networkManager);

    virtual bool uptareMangaList(UpdateProgressToken *token) = 0;

    virtual void updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                                QSharedPointer<MangaInfo> mangainfo) = 0;

    virtual Result<QSharedPointer<MangaInfo>, QString> getMangaInfo(const QString &mangaUrl);

    virtual Result<QStringList, QString> getPageList(const QString &chapterUrl) = 0;
    virtual Result<QString, QString> getImageUrl(const QString &pageUrl);

    Result<QSharedPointer<MangaInfo>, QString> loadMangaInfo(const QString &mangaUrl,
                                                             const QString &mangatitle, bool update = true);

    bool serializeMangaList();
    bool deserializeMangaList();

    QString getImagePath(const DownloadImageDescriptor &descriptor);

    QSharedPointer<DownloadFileJob> downloadImage(const DownloadImageDescriptor &descriptor);

    Result<QString, QString> downloadAwaitImage(const DownloadImageDescriptor &descriptor);

    QString htmlToPlainText(const QString &str);

    virtual void updateMangaInfoAsync(QSharedPointer<MangaInfo> mangainfo);
    void downloadCoverAsync(QSharedPointer<MangaInfo> mangainfo);
    Result<void, QString> updatePageList(QSharedPointer<MangaInfo> info, int chapter);

protected:
    QByteArray mangaInfoPostDataStr;

    NetworkManager *networkManager;
    QTextDocument htmlConverter;

    void genrateCoverThumbnail(QSharedPointer<MangaInfo> mangainfo);
    void fillMangaInfo(QSharedPointer<MangaInfo> info, const QString &buffer,
                       const QRegularExpression &titlerx, const QRegularExpression &authorrx,
                       const QRegularExpression &artistrx, const QRegularExpression &statusrx,
                       const QRegularExpression &yearrx, const QRegularExpression &genresrx,
                       const QRegularExpression &summaryrx, const QRegularExpression &coverrx);
};

#endif  // ABSTRACTMANGASOURCE_H
