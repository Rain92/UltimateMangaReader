#ifndef DOWNLOADSTRINGJOB_H
#define DOWNLOADSTRINGJOB_H

#include "downloadbufferjob.h"

class DownloadStringJob : public DownloadBufferJob
{
    Q_OBJECT

protected:
    void downloadStringFinished();

public:
    QString bufferStr;

    DownloadStringJob(QNetworkAccessManager *networkManager, const QString &url, int timeout = 6000,
                      const QByteArray &postData = QByteArray(),
                      const QList<std::tuple<const char *, const char *>> &customHeaders = {});

    void start() override;
    void restart() override;
};

#endif  // DOWNLOADFILEJOB_H
