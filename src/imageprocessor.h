#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QThread>

#include "enums.h"

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

struct ImageProcessingRequest
{
    QByteArray data;
    QString filepath;
    QSize screenSize;
    DoublePageMode doublePageMode = DoublePageNoRotation;
    bool trim = false;
    bool manhwaMode = false;
    bool swDither = false;
    EncryptionDescriptor encryption;
};

// Turns a downloaded page into the screen sized grayscale image and saves it: decrypt, decode, rotate,
// trim, rescale, encode.
QImage processDownloadedImage(const ImageProcessingRequest &request);

// Runs processDownloadedImage() on its own (low priority) thread, one request at a time, so the gui
// thread stays responsive while pages are preloaded. Results are reported with finished(), queued to
// the gui thread.
class ImageProcessor : public QObject
{
    Q_OBJECT

public:
    explicit ImageProcessor(QObject *parent = nullptr);
    ~ImageProcessor();

    // Returns the id finished() will report the result with.
    quint64 process(const ImageProcessingRequest &request);

signals:
    void finished(quint64 id, const QImage &image);

private:
    QThread thread;
    QObject worker;
    quint64 nextId;
};

#endif  // IMAGEPROCESSOR_H
