#ifndef DOWNLOADIMAGEANDRESCALEJOB_H
#define DOWNLOADIMAGEANDRESCALEJOB_H

#include <QImage>

#include "downloadfilejob.h"
#include "imageprocessingnative.h"
#include "imageprocessingqt.h"
#include "settings.h"
#include "utils.h"

enum EncryptionType
{
    NoEncryption = 0,
    XorEncryption
};

struct EncryptionDescriptor
{
    EncryptionType type = NoEncryption;
    QByteArray key = {};
};

class DownloadScaledImageJob : public DownloadFileJob
{
    Q_OBJECT

public:
    DownloadScaledImageJob(QNetworkAccessManager *networkManager, const QString &url, const QString &path,
                           QSize screenSize, Settings *settings,
                           const QList<std::tuple<const char *, const char *>> &customHeaders = {},
                           const EncryptionDescriptor &encryption = {});
    virtual ~DownloadScaledImageJob() = default;

    void downloadFileReadyRead() override;
    void downloadFileFinished() override;

    QSharedPointer<QImage> resultImage;

private:
    QSize screenSize;
    Settings *settings;
    EncryptionDescriptor encryption;

    bool processImage(QByteArray &&array);
};

#endif  // DOWNLOADIMAGEANDRESCALEJOB_H
