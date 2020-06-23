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

protected:
    virtual void paintEvent(QPaintEvent *);

private:
    bool showError;
    QSharedPointer<QPixmap> pixmap;
    QSharedPointer<QPixmap> errorPixmap;
};

#endif  // MANGAIMAGECONTAINER_H
