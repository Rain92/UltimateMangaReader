#ifndef MANGAIMAGECONTAINER_H
#define MANGAIMAGECONTAINER_H

#include <QElapsedTimer>
#include <QFrame>
#include <QMouseEvent>
#include <QPainter>

class MangaImageContainer : public QFrame
{
    Q_OBJECT

public:
    explicit MangaImageContainer(QWidget *parent);

    void setImage(QSharedPointer<QImage> img);
    void clearImage();
    void showErrorImage();
    void setVOffset(int y);

protected:
    virtual void paintEvent(QPaintEvent *) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;

private:
    int lastY;
    int vOffset;
    bool showError;
    QSharedPointer<QImage> image;
    QSharedPointer<QImage> errorImage;
};

#endif  // MANGAIMAGECONTAINER_H
