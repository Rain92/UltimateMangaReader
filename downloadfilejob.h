#ifndef DOWNLOADFILEJOB_H
#define DOWNLOADFILEJOB_H

#include <QtCore>
#include <QtNetwork>
#include <QSslError>

class DownloadFileJob : public QObject
{
    Q_OBJECT

public:
    DownloadFileJob(QObject *parent, QNetworkAccessManager *nm, const QString &url, const QString &path);

    QUrl url;
    QString filepath;
    bool isCompleted;
    QNetworkAccessManager *networkManager;
    QString errorString;
    QFile file;
    QNetworkReply *reply;

    bool await(int timeout = 10000);

signals:
    void completed();
    void downloadError();

public slots:
    virtual void downloadFileReadyRead();
    virtual void downloadFileFinished();
    virtual void onSslErrors(const QList<QSslError> &);
    virtual void onError(QNetworkReply::NetworkError code);



private:
};

#endif // DOWNLOADFILEJOB_H
