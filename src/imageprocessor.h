#pragma once

#include <QAtomicInt>
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
    // the rest of the page, which the source cut off into another image (stitched below data)
    QByteArray appendData;
    QString filepath;
    QSize screenSize;
    DoublePageMode doublePageMode = DoublePageNoRotation;
    bool trim = false;
    bool manhwaMode = false;
    EncryptionDescriptor encryption;
};

// Turns a downloaded page into the screen sized grayscale image: decrypt, decode, rotate, trim, rescale.
QImage processDownloadedImage(const ImageProcessingRequest &request);

// Runs processDownloadedImage() on its own (low priority) thread, one request at a time, so the gui
// thread stays responsive while pages are preloaded, and saves the page (to request.filepath). Results are
// reported with processed() and finished(), queued to the gui thread.
class ImageProcessor : public QObject
{
    Q_OBJECT

public:
    explicit ImageProcessor(QObject *parent = nullptr);
    ~ImageProcessor();

    // Returns the id finished() will report the result with.
    quint64 process(const ImageProcessingRequest &request);

    // True while requests are queued or being processed.
    bool isBusy() const;

signals:
    // The page is processed (a null image if that failed), it is saved next.
    void processed(quint64 id, const QImage &image);
    // The page is saved, a null image if it couldn't be processed or saved.
    void finished(quint64 id, const QImage &image);

private:
    QThread thread;
    QObject worker;
    quint64 nextId;
    QAtomicInt pending;
};
