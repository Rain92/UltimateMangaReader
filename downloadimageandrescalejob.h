#ifndef DOWNLOADIMAGEANDRESCALEJOB_H
#define DOWNLOADIMAGEANDRESCALEJOB_H

#include "downloadfilejob.h"


class DownloadScaledImageJob : public DownloadFileJob
{
    Q_OBJECT

public:
    DownloadScaledImageJob(QObject *parent, QNetworkAccessManager *nm, const QString &url, const QString &path, int width, int height);

    int width;
    int height;

signals:

public slots:
    void downloadFileReadyRead();
    void downloadFileFinished();

private slots:
    void rescaleImageFinised();

private:
    void rescaleImage(QByteArray array, QString filename);
    QFutureWatcher<void> watcher;
    QByteArray array;
};

#endif // DOWNLOADIMAGEANDRESCALEJOB_H
