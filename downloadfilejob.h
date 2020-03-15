#ifndef DOWNLOADFILEJOB_H
#define DOWNLOADFILEJOB_H

#include "downloadjobbase.h"

class DownloadFileJob : public DownloadJobBase
{
    Q_OBJECT

protected:
    QFile file;

public:
    DownloadFileJob(QObject *parent, QNetworkAccessManager *networkManager,
                    const QString &url, const QString &localFilePath);
    ~DownloadFileJob();

    QString filepath;

    bool await(int timeout = 3000);

public slots:
    void start() override;
    void restart() override;
    virtual void downloadFileReadyRead();
    virtual void downloadFileFinished();
    void onError(QNetworkReply::NetworkError);
};

#endif  // DOWNLOADFILEJOB_H
