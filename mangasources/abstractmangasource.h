#ifndef ABSTRACTMANGASOURCE_H
#define ABSTRACTMANGASOURCE_H
#include <QDateTime>
#include <QRegularExpression>
#include <QTextDocument>

#include "downloadmanager.h"
//#include "mangainfo.h"
#include "mangalist.h"
#include "utils.h"

class MangaInfo;

class AbstractMangaSource : public QObject
{
    Q_OBJECT
public:
    QString name;

    QString baseurl;

    MangaList mangalist;

    AbstractMangaSource(QObject *parent, DownloadManager *downloadmanager);

    virtual void initialize() {}

    virtual MangaList getMangaList() = 0;

    virtual void updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job,
        QSharedPointer<MangaInfo> mangainfo) = 0;

    virtual QSharedPointer<MangaInfo> getMangaInfo(
        const QString &mangalink) = 0;

    virtual QStringList getPageList(const QString &chapterlink) = 0;
    virtual QString getImageLink(const QString &pagelink) = 0;

    QSharedPointer<MangaInfo> loadMangaInfo(const QString &mangalink,
                                            const QString &mangatitle,
                                            bool update = true);

    bool serializeMangaList();
    bool deserializeMangaList();

    QSharedPointer<DownloadFileJob> downloadImage(const QString &imagelink,
                                                  const QString &mangatitle,
                                                  const int &chapternum,
                                                  const int &pagenum);
    QString downloadAwaitImage(const QString &imagelink,
                               const QString &mangatitle, const int &chapternum,
                               const int &pagenum);

    QString htmlToPlainText(const QString &str);

public slots:
    virtual void updateMangaInfo(QSharedPointer<MangaInfo> mangainfo);
    virtual void downloadCover(QSharedPointer<MangaInfo> mangainfo);

signals:
    void updateProgress(int);
    void updateError(QString);

protected:
    DownloadManager *downloadmanager;
    QTextDocument htmlconverter;
};

#endif  // ABSTRACTMANGASOURCE_H
