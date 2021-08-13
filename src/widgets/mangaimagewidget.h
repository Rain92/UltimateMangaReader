#ifndef MANGAIMAGEWIDGETH_H
#define MANGAIMAGEWIDGETH_H

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFrame>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

#ifdef KOBO
#include "koboplatformfunctions.h"
#endif

class MangaImageWidget : public QFrame
{
    Q_OBJECT

public:
    explicit MangaImageWidget(QWidget *parent);

    void setImage(QSharedPointer<QImage> img);
    void clearImage();
    void showErrorImage();
    void setVOffset(int y);

protected:
    virtual void paintEvent(QPaintEvent *) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;

private:
    int lastY;
    int vOffset;
    bool showError;
    QSharedPointer<QImage> image;
    QSharedPointer<QImage> errorImage;
};

#endif  // MANGAIMAGEWIDGETH_H
