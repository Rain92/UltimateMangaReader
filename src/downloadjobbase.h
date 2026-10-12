#ifndef DOWNLOADJOBBASE_H
#define DOWNLOADJOBBASE_H

#include <QTime>
#include <QtNetwork>

class DownloadJobBase : public QObject
{
    Q_OBJECT

protected:
    QNetworkAccessManager *networkManager;
    std::unique_ptr<QNetworkReply> reply;
    QPointer<QNetworkAccessManager> managerGuard;
    QList<std::tuple<const char *, const char *>> customHeaders;

    QNetworkRequest createRequest() const;

signals:
    void completed();
    void downloadError();

public:
    DownloadJobBase(QNetworkAccessManager *networkManager, const QString &url,
                    const QList<std::tuple<const char *, const char *>> &customHeaders = {});
    virtual ~DownloadJobBase();

    QString url;
    QString originalUrl;
    bool isCompleted;
    QString errorString;

    QList<QNetworkCookie> getCookies();
    bool hasFailed() const;

    // Counts reply as active until it finished or got destroyed.
    static void trackReply(QNetworkReply *reply);
    // Number of tracked network replies that are still running.
    static int activeReplies();

    virtual void start() = 0;
    virtual void restart() = 0;
    virtual void onSslErrors(const QList<QSslError> &);
};

#endif  // DOWNLOADJOBBASE_H
