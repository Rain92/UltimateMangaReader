#ifndef ABSTRACTMANGASOURCE_H
#define ABSTRACTMANGASOURCE_H
#include <qdatetime.h>
#include <qstringlist.h>

#include <QRegularExpression>
#include <QTextDocument>

#include "downloadmanager.h"

class MangaInfo;

struct MangaList
{
    QStringList titles;
    QStringList links;
    bool isAbsoluteUrl = false;
    int nominalSize = 0;
    int actualSize = 0;

    void sort()
    {
        qSort(links.begin(), links.end(), qGreater<int>());
        titles.sort
    }
};

class AbstractMangaSource : public QObject
{
    Q_OBJECT
public:
    QString name;

    QString baseurl;

    MangaList mangalist;
    int nummangas;

    AbstractMangaSource(QObject *parent, DownloadManager *downloadmanager);

    virtual void initialize() {}

    virtual MangaList updateMangaList() = 0;

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
    void updateMangaInfo(QSharedPointer<MangaInfo> mangainfo);

signals:
    void updateProgress(int);
    void updateError(QString);

protected:
    DownloadManager *downloadmanager;
    QTextDocument htmlconverter;
};

#endif  // ABSTRACTMANGASOURCE_H
