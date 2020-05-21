#ifndef DOWNLOADSTRINGJOB_H
#define DOWNLOADSTRINGJOB_H

#include "downloadjobbase.h"

class DownloadStringJob : public DownloadJobBase
{
    Q_OBJECT

protected:
    QTimer timeoutTimer;
    int timeoutTime;
    QByteArray postData;

protected:
    void downloadStringReadyRead();
    void downloadStringFinished();
    void onError(QNetworkReply::NetworkError);
    void timeout();

public:
    QString buffer;

    DownloadStringJob(QNetworkAccessManager *networkManager, const QString &url, int timeout = 6000,
                      const QByteArray &postData = QByteArray(),
                      const QList<std::tuple<const char *, const char *>> &customHeaders = {});

    bool await(int timeout = 7000, bool retry = true);

public:
    void start() override;
    void restart() override;
};

#endif  // DOWNLOADFILEJOB_H
