#pragma once

#include "downloadjobbase.h"

// Downloads into a file (complete when the file is there, an existing file isn't downloaded again).
class DownloadFileJob : public DownloadJobBase
{
    Q_OBJECT

public:
    DownloadFileJob(QNetworkAccessManager *networkManager, const DownloadRequest &request,
                    const QString &localFilePath);
    virtual ~DownloadFileJob() = default;

    const QString filepath;

    // How file downloads try by default: no time limit (files can be large), but stalled ones fail (a
    // DownloadQueue would wait forever), and errors that may go away are retried twice.
    static DownloadRequest defaultRequest(const QString &url, const HttpHeaders &headers = {});

protected:
    void startAttempt() override;
    // Prepares the target before the request is sent, e.g. opens the file. Returns an error message.
    virtual QString prepareTarget();
    virtual void readyRead();
    // The request was answered without an error.
    virtual void replySucceeded();
    void replyFinished();

private:
    // written while the download runs, renamed when it's complete
    QFile partFile;
};
