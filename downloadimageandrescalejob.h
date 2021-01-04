#ifndef DOWNLOADIMAGEANDRESCALEJOB_H
#define DOWNLOADIMAGEANDRESCALEJOB_H

#include <QImage>

#include "downloadfilejob.h"
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
                           QSize imgSize, Settings *settings,
                           const QList<std::tuple<const char *, const char *>> &customHeaders = {},
                           EncryptionDescriptor encryption = {});

    void downloadFileReadyRead() override;
    void downloadFileFinished() override;

private:
    QSize imgSize;
    Settings *settings;
    EncryptionDescriptor encryption;

    bool processImage(QByteArray &&array);
    QImage rescaleImage(const QImage &img);
    QRect getTrimRect(const QImage &image);
};

#endif  // DOWNLOADIMAGEANDRESCALEJOB_H
