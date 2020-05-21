#ifndef DOWNLOADIMAGEANDRESCALEJOB_H
#define DOWNLOADIMAGEANDRESCALEJOB_H

#include <QImage>

#include "downloadfilejob.h"

class DownloadScaledImageJob : public DownloadFileJob
{
    Q_OBJECT

public:
    DownloadScaledImageJob(QNetworkAccessManager *networkManager, const QString &url, const QString &path,
                           QSize imgSize,
                           const QList<std::tuple<const char *, const char *>> &customHeaders = {});

signals:

public slots:
    virtual void downloadFileReadyRead() override;
    virtual void downloadFileFinished() override;

private:
    QSize imgSize;
    bool rescaleImage(const QByteArray &array);
};

#endif  // DOWNLOADIMAGEANDRESCALEJOB_H
