#ifndef DOWNLOADSTRINGJOB_H
#define DOWNLOADSTRINGJOB_H

#include "downloadbufferjob.h"

// Downloads a text (UTF-8).
class DownloadStringJob : public DownloadBufferJob
{
    Q_OBJECT

public:
    QString bufferStr;

    DownloadStringJob(QNetworkAccessManager *networkManager, const QString &url, int timeout = 6000,
                      const HttpHeaders &headers = {});
    virtual ~DownloadStringJob() = default;

    void restart() override;

protected:
    void downloadFinished() override;
};

#endif  // DOWNLOADSTRINGJOB_H
