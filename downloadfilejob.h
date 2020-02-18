#ifndef DOWNLOADFILEJOB_H
#define DOWNLOADFILEJOB_H

#include <QtCore>
#include <QtNetwork>
#include <QSslError>

class DownloadFileJob : public QObject
{
    Q_OBJECT

protected:
    QNetworkAccessManager *networkManager;
    QFile file;
    QNetworkReply *reply;

public:
    DownloadFileJob(QObject *parent, QNetworkAccessManager *nm, const QString &url, const QString &localFilePath);
    ~DownloadFileJob();

    QUrl url;
    QString filepath;
    bool isCompleted;
    QString errorString;

    bool await(int timeout = 10000);

signals:
    void completed();
    void downloadError();

public slots:
    virtual void downloadFileReadyRead();
    virtual void downloadFileFinished();
    virtual void onSslErrors(const QList<QSslError> &errors);
    virtual void onError(QNetworkReply::NetworkError);

};

#endif // DOWNLOADFILEJOB_H
