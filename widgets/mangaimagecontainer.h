#ifndef MANGAIMAGECONTAINER_H
#define MANGAIMAGECONTAINER_H

#include <qframe.h>

#include <QMouseEvent>
#include <QPainter>

class MangaImageContainer : public QFrame
{
    Q_OBJECT

public:
    explicit MangaImageContainer(QWidget *parent);

    void setImage(QSharedPointer<QPixmap> img);
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
    QSharedPointer<QPixmap> pixmap;
    QSharedPointer<QPixmap> errorPixmap;
};

#endif  // MANGAIMAGECONTAINER_H
