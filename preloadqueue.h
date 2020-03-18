#ifndef PRELOADQUEUE_H
#define PRELOADQUEUE_H

#include <QObject>
#include <QQueue>

#include "abstractmangasource.h"
#include "downloadimageandrescalejob.h"
#include "downloadimagedescriptor.h"

class PreloadQueue : public QObject
{
    Q_OBJECT

public:
    explicit PreloadQueue(AbstractMangaSource *source);

    void addJob(const DownloadImageDescriptor &info);
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
    QQueue<DownloadImageDescriptor> queue;
    QSharedPointer<DownloadFileJob> job;
    QTimer resettimer;
    bool running;
};

#endif  // PRELOADQUEUE_H
