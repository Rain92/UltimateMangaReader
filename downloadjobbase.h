#ifndef DOWNLOADJOBBASE_H
#define DOWNLOADJOBBASE_H

#include <QTime>
#include <QtNetwork>

class DownloadJobBase : public QObject
{
    Q_OBJECT

protected:
    QNetworkAccessManager *networkManager;
    QScopedPointer<QNetworkReply> reply;

public:
    DownloadJobBase(QNetworkAccessManager *networkManager, const QString &url);

    QString url;
    QString originalUrl;
    bool isCompleted;
    QString errorString;

    QList<QNetworkCookie> getCookies();

signals:
    void completed();
    void downloadError();

public slots:
    virtual void start() = 0;
    virtual void restart() = 0;
    virtual void onSslErrors(const QList<QSslError> &);
};

#endif  // DOWNLOADJOBBASE_H
