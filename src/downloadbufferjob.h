#ifndef DOWNLOADBUFFERJOB_H
#define DOWNLOADBUFFERJOB_H

#include "downloadjobbase.h"

class DownloadBufferJob : public DownloadJobBase
{
    Q_OBJECT

protected:
    QTimer timeoutTimer;
    int timeoutTime;

    virtual void downloadFinished();
    void onError(QNetworkReply::NetworkError);
    void timeout();

public:
    QByteArray buffer;
    int httpStatus;

    DownloadBufferJob(QNetworkAccessManager *networkManager, const QString &url, int timeout = 6000,
                      const QList<std::tuple<const char *, const char *>> &customHeaders = {});
    virtual ~DownloadBufferJob() = default;

    bool await(int timeout = 7000, bool retry = true, int maxRetries = 10);

    void start() override;
    void restart() override;
};

#endif  // DOWNLOADBUFFERJOB_H
