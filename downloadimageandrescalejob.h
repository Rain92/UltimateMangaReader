#ifndef DOWNLOADIMAGEANDRESCALEJOB_H
#define DOWNLOADIMAGEANDRESCALEJOB_H

#include "downloadfilejob.h"

class DownloadScaledImageJob : public DownloadFileJob
{
    Q_OBJECT

public:
    DownloadScaledImageJob(QObject *parent,
                           QNetworkAccessManager *networkManager,
                           const QString &url, const QString &path, QSize size);

    QSize size;

signals:

public slots:
    virtual void downloadFileReadyRead() override;
    virtual void downloadFileFinished() override;

private slots:
    void rescaleImageFinised();

private:
    void rescaleImage(const QByteArray &array, const QString &filename);
    QFutureWatcher<void> watcher;
    QByteArray array;
};

#endif  // DOWNLOADIMAGEANDRESCALEJOB_H
