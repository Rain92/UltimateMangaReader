#ifndef DOWNLOADIMAGEANDRESCALEJOB_H
#define DOWNLOADIMAGEANDRESCALEJOB_H

#include <QImage>

#include "downloadfilejob.h"
#include "imageprocessor.h"
#include "settings.h"

class DownloadScaledImageJob : public DownloadFileJob
{
    Q_OBJECT

public:
    DownloadScaledImageJob(QNetworkAccessManager *networkManager, ImageProcessor *imageProcessor,
                           const QString &url, const QString &path, QSize screenSize, Settings *settings,
                           const QList<std::tuple<const char *, const char *>> &customHeaders = {},
                           const EncryptionDescriptor &encryption = {});
    virtual ~DownloadScaledImageJob() = default;

    void downloadFileReadyRead() override;
    void downloadFileFinished() override;

    QSharedPointer<QImage> resultImage;

private:
    ImageProcessor *imageProcessor;
    QSize screenSize;
    Settings *settings;
    EncryptionDescriptor encryption;
    quint64 processingId;

    void processingFinished(quint64 id, const QImage &image);
};

#endif  // DOWNLOADIMAGEANDRESCALEJOB_H
