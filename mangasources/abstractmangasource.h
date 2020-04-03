#ifndef ABSTRACTMANGASOURCE_H
#define ABSTRACTMANGASOURCE_H
#include <QDateTime>
#include <QImage>
#include <QRegularExpression>
#include <QTextDocument>

#include "downloadimagedescriptor.h"
#include "downloadmanager.h"
#include "downloadqueue.h"
#include "mangachapter.h"
#include "mangalist.h"
#include "result.h"
#include "sizes.h"
#include "staticsettings.h"

class MangaInfo;

class AbstractMangaSource : public QObject
{
    Q_OBJECT
public:
    QString name;

    QString baseurl;

    MangaList mangaList;

    AbstractMangaSource(QObject *parent, DownloadManager *downloadManager);

    virtual MangaList getMangaList() = 0;

    virtual void updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job,
        QSharedPointer<MangaInfo> mangainfo) = 0;

    virtual Result<QSharedPointer<MangaInfo>, QString> getMangaInfo(
        const QString &mangalink);

    virtual Result<QStringList, QString> getPageList(
        const QString &chapterlink) = 0;
    virtual Result<QString, QString> getImageLink(const QString &pagelink);

    Result<QSharedPointer<MangaInfo>, QString> loadMangaInfo(
        const QString &mangalink, const QString &mangatitle,
        bool update = true);

    bool serializeMangaList();
    bool deserializeMangaList();

    QString getImagePath(const DownloadImageDescriptor &descriptor);

    QSharedPointer<DownloadFileJob> downloadImage(
        const DownloadImageDescriptor &descriptor);

    Result<QString, QString> downloadAwaitImage(
        const DownloadImageDescriptor &descriptor);

    QString htmlToPlainText(const QString &str);

public slots:

    virtual void updateMangaInfoAsync(QSharedPointer<MangaInfo> mangainfo);
    void downloadCoverAsync(QSharedPointer<MangaInfo> mangainfo);
    Result<void, QString> updatePageList(QSharedPointer<MangaInfo> info,
                                         int chapter);

signals:
    void updateProgress(int);
    void updateError(QString);

protected:
    DownloadManager *downloadManager;
    QTextDocument htmlConverter;

    void genrateCoverThumbnail(QSharedPointer<MangaInfo> mangainfo);
    void fillMangaInfo(
        QSharedPointer<MangaInfo> info, const QString &buffer,
        const QRegularExpression &titlerx, const QRegularExpression &authorrx,
        const QRegularExpression &artistrx, const QRegularExpression &statusrx,
        const QRegularExpression &yearrx, const QRegularExpression &genresrx,
        const QRegularExpression &summaryrx, const QRegularExpression &coverrx);
};

#endif  // ABSTRACTMANGASOURCE_H
