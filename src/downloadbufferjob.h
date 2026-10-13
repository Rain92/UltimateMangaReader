#pragma once

#include "downloadjobbase.h"

// Downloads into memory.
class DownloadBufferJob : public DownloadJobBase
{
    Q_OBJECT

public:
    DownloadBufferJob(QNetworkAccessManager *networkManager, const DownloadRequest &request);
    virtual ~DownloadBufferJob() = default;

    // the body of the response, also of a failed one
    QByteArray buffer;
    // The body as UTF-8 text, decoded on every call.
    QString text() const { return QString::fromUtf8(buffer); }

protected:
    void startAttempt() override;
    void resetState() override;

private:
    void readyRead();
    void replyFinished();

};
