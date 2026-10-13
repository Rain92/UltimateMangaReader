#ifndef MANGAIMAGEWIDGET_H
#define MANGAIMAGEWIDGET_H

#include <QFrame>
#include <QImage>
#include <QSharedPointer>

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
    QSize displaySize(const QImage &img, QSize deviceSize, bool *tall = nullptr) const;
    QSize deviceSize() const;

    int lastY;
    int vOffset;
    bool showError;
    QSharedPointer<QImage> image;
    QSharedPointer<QImage> errorImage;
};

#endif  // MANGAIMAGEWIDGET_H
