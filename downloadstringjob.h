#ifndef DOWNLOADSTRINGJOB_H
#define DOWNLOADSTRINGJOB_H

#include <QtNetwork>
#include <QTime>

class DownloadStringJob : public QObject
{
    Q_OBJECT

public:
    DownloadStringJob(QObject *parent, QNetworkAccessManager *nm, const QString &url);
    ~DownloadStringJob();
    QUrl url;
    bool isCompleted;
    QNetworkAccessManager *networkManager;
    QString errorString;
    QString buffer;
    QNetworkReply *reply;

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

private:
};

#endif // DOWNLOADFILEJOB_H
