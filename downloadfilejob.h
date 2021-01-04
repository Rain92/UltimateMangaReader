#ifndef DOWNLOADFILEJOB_H
#define DOWNLOADFILEJOB_H

#include "downloadjobbase.h"

class DownloadFileJob : public DownloadJobBase
{
    Q_OBJECT

protected:
    QFile file;

    virtual void downloadFileReadyRead();
    virtual void downloadFileFinished();

public:
    DownloadFileJob(QNetworkAccessManager *networkManager, const QString &url, const QString &localFilePath,
                    const QList<std::tuple<const char *, const char *>> &customHeaders = {});
    ~DownloadFileJob();

    QString filepath;

    bool await(int timeout = 7000);

    void start() override;
    void restart() override;
    void onError(QNetworkReply::NetworkError);
};

#endif  // DOWNLOADFILEJOB_H
