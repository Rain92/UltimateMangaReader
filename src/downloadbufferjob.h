#ifndef DOWNLOADBUFFERJOB_H
#define DOWNLOADBUFFERJOB_H

#include "downloadjobbase.h"

// Downloads into memory.
class DownloadBufferJob : public DownloadJobBase
{
    Q_OBJECT

public:
    QByteArray buffer;

    // timeout: for the download (it fails then), unless an await() sets another one
    DownloadBufferJob(QNetworkAccessManager *networkManager, const QString &url, int timeout = 6000,
                      const HttpHeaders &headers = {});
    virtual ~DownloadBufferJob() = default;

    // Waits until the download is done, at most timeout ms (the download is stopped then). With retry it starts
    // over (up to maxRetries times, within the timeout) after errors that aren't the request's fault.
    bool await(int timeout = 7000, bool retry = true, int maxRetries = 10);

    void start() override;
    void restart() override;

protected:
    QTimer timeoutTimer;
    int timeoutTime;

    virtual void downloadFinished();
    void timeout();
};

#endif  // DOWNLOADBUFFERJOB_H
