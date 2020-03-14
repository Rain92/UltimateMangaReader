#ifndef DOWNLOADSTRINGJOB_H
#define DOWNLOADSTRINGJOB_H

#include "downloadjobbase.h"

class DownloadStringJob : public DownloadJobBase
{
    Q_OBJECT

protected:
    QTimer timeouttimer;
    int timeouttime;
    QByteArray *postdata;

public:
    QString buffer;

    DownloadStringJob(QObject *parent, QNetworkAccessManager *networkManager,
                      const QString &url, int timeout = 3000,
                      QByteArray *postdata = nullptr);

    bool await(int timeout = 5000, bool retry = true);

public slots:
    void start() override;
    void restart() override;
    void downloadStringReadyRead();
    void downloadStringFinished();
    void onError(QNetworkReply::NetworkError);
    void timeout();
};

#endif  // DOWNLOADFILEJOB_H
