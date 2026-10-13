#ifndef DOWNLOADIMAGEANDRESCALEJOB_H
#define DOWNLOADIMAGEANDRESCALEJOB_H

#include <QImage>

#include "downloadfilejob.h"
#include "imageprocessor.h"
#include "settings.h"

// Downloads a page and saves it processed for the screen (see processDownloadedImage()).
class DownloadScaledImageJob : public DownloadFileJob
{
    Q_OBJECT

public:
    DownloadScaledImageJob(QNetworkAccessManager *networkManager, ImageProcessor *imageProcessor,
                           const QString &url, const QString &path, QSize screenSize, Settings *settings,
                           const HttpHeaders &headers = {}, const EncryptionDescriptor &encryption = {},
                           const QString &appendUrl = {});
    virtual ~DownloadScaledImageJob();

    void restart() override;

    // the processed page, when it was downloaded (not when the file was there already)
    QSharedPointer<QImage> resultImage;

protected:
    QString prepareTarget() override;
    void readyRead() override;
    void replySucceeded() override;

private:
    ImageProcessor *imageProcessor;
    QSize screenSize;
    Settings *settings;
    EncryptionDescriptor encryption;
    quint64 processingId;

    // the rest of the page, cut off by the source into another image: it is downloaded after the page and
    // stitched below it, without it if it can't be loaded
    QString appendUrl;
    std::unique_ptr<QNetworkReply> appendReply;
    int appendRetries;
    QByteArray pageData;

    void downloadAppendix();
    void appendixFinished();
    void process(const QByteArray &appendData);
    void processingFinished(quint64 id, const QImage &image);
};

#endif  // DOWNLOADIMAGEANDRESCALEJOB_H
