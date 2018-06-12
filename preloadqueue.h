#ifndef PRELOADQUEUE_H
#define PRELOADQUEUE_H

#include <QQueue>
#include <QObject>
#include "downloadimageandrescalejob.h"
#include "abstractmangasource.h"

struct DownloadImageInfo
{
    DownloadImageInfo(QString imagelink, QString title, int chapter, int page):
        imagelink(imagelink),
        title(title),
        chapter(chapter),
        page(page)
    { }

    QString imagelink;
    QString title;
    int chapter;
    int page;
};

class PreloadQueue : public QObject
{
    Q_OBJECT

public:
    PreloadQueue(QObject *parent, AbstractMangaSource *source);

    void addJob(DownloadImageInfo info);
    void clearQuene();

    DownloadScaledImageJob *currentJob();

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
    DownloadScaledImageJob *job;
    QTimer *resettimer;
    bool running;
};

#endif // PRELOADQUEUE_H
