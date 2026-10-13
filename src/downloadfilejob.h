#ifndef DOWNLOADFILEJOB_H
#define DOWNLOADFILEJOB_H

#include "downloadjobbase.h"

// Downloads into a file (complete when the file is there, an existing file isn't downloaded again). Errors that
// may go away are retried twice.
class DownloadFileJob : public DownloadJobBase
{
    Q_OBJECT

public:
    DownloadFileJob(QNetworkAccessManager *networkManager, const QString &url, const QString &localFilePath,
                    const HttpHeaders &headers = {});
    virtual ~DownloadFileJob() = default;

    const QString filepath;

    // Waits until the download is done, at most timeout ms. The download goes on after a timeout (others may
    // wait for the file too), the result is false then.
    bool await(int timeout = 7000);

    void start() override;
    void restart() override;

protected:
    // Prepares the target before the request is sent, e.g. opens the file. Returns an error message.
    virtual QString prepareTarget();
    virtual void readyRead();
    // The request was answered without an error.
    virtual void replySucceeded();
    void replyFinished();

private:
    // written while the download runs, renamed when it's complete
    QFile partFile;
    int retries;
};

#endif  // DOWNLOADFILEJOB_H
