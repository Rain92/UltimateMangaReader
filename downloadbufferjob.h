#ifndef DOWNLOADBUFFERJOB_H
#define DOWNLOADBUFFERJOB_H

#include "downloadjobbase.h"

class DownloadBufferJob : public DownloadJobBase
{
    Q_OBJECT

protected:
    QTimer timeoutTimer;
    int timeoutTime;
    QByteArray postData;

    void downloadReadyRead();
    virtual void downloadFinished();
    void onError(QNetworkReply::NetworkError);
    void timeout();

public:
    QByteArray buffer;

    DownloadBufferJob(QNetworkAccessManager *networkManager, const QString &url, int timeout = 6000,
                      const QByteArray &postData = QByteArray(),
                      const QList<std::tuple<const char *, const char *>> &customHeaders = {});

    bool await(int timeout = 7000, bool retry = true);

    void start() override;
    void restart() override;
};

#endif  // DOWNLOADBUFFERJOB_H
