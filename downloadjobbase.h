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
    QList<std::tuple<const char *, const char *>> customHeaders;

public:
    DownloadJobBase(QNetworkAccessManager *networkManager, const QString &url,
                    const QList<std::tuple<const char *, const char *>> &customHeaders = {});

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
