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

public:
    QString buffer;

    DownloadStringJob(QNetworkAccessManager *networkManager, const QString &url,
                      int timeout = 6000,
                      const QByteArray &postData = QByteArray());

    bool await(int timeout = 6000, bool retry = true);

public slots:
    void start() override;
    void restart() override;
    void downloadStringReadyRead();
    void downloadStringFinished();
    void onError(QNetworkReply::NetworkError);
    void timeout();
};

#endif  // DOWNLOADFILEJOB_H
