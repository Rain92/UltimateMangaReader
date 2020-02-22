#ifndef DOWNLOADSTRINGJOB_H
#define DOWNLOADSTRINGJOB_H

#include <QTime>
#include <QtNetwork>

class DownloadStringJob : public QObject
{
    Q_OBJECT

protected:
    QNetworkAccessManager *networkManager;
    QScopedPointer<QNetworkReply> reply;

public:
    QUrl url;
    bool isCompleted;
    QString errorString;
    QString buffer;
    QTimer timeouttimer;
    int timeouttime;

    DownloadStringJob(QObject *parent, QNetworkAccessManager *networkManager,
                      const QString &url, int timeout = 3000);

    bool await(int timeout = 5000, bool retry = true);

signals:
    void completed();
    void downloadError();

public slots:
    void start();
    void restart();
    void downloadStringReadyRead();
    void downloadStringFinished();
    void onSslErrors(const QList<QSslError> &);
    void onError(QNetworkReply::NetworkError);
    void timeout();
};

#endif  // DOWNLOADFILEJOB_H
