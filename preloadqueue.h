#ifndef PRELOADQUEUE_H
#define PRELOADQUEUE_H

#include <QObject>
#include <QQueue>

#include "abstractmangasource.h"
#include "downloadimageandrescalejob.h"

struct DownloadImageInfo
{
    DownloadImageInfo(QString imagelink, QString title, int chapter, int page)
        : imagelink(imagelink), title(title), chapter(chapter), page(page)
    {
    }

    QString imagelink;
    QString title;
    int chapter;
    int page;
    QSharedPointer<DownloadFileJob> job;
};

class PreloadQueue : public QObject
{
    Q_OBJECT

public:
    PreloadQueue(QObject *parent, AbstractMangaSource *source);

    void addJob(DownloadImageInfo info);
    void clearQuene();

    QSharedPointer<DownloadFileJob> currentJob();

signals:
    void completedDownload(const QString &path);

public slots:
    void processNext();

private slots:
    void sendComletedSignal();
    void resetQueue();

private:
    AbstractMangaSource *source;
    QQueue<DownloadImageInfo> queue;
    QSharedPointer<DownloadFileJob> job;
    QTimer *resettimer;
    bool running;
};

#endif  // PRELOADQUEUE_H
