#ifndef DOWNLOADSTRINGJOB_H
#define DOWNLOADSTRINGJOB_H

#include <QtNetwork>
#include <QTime>

class DownloadStringJob : public QObject
{
    Q_OBJECT

public:
    DownloadStringJob(QObject *parent, QNetworkAccessManager *nm, const QString &url, int timeout = 3000);


    QUrl url;
    bool isCompleted;
    QNetworkAccessManager *networkManager;
    QString errorString;
    QString buffer;
    QNetworkReply *reply;

    bool await(int timeout = 5000, bool retry = true);

    QTimer timeouttimer;
    int timeouttime;

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

private:
};

#endif // DOWNLOADFILEJOB_H
