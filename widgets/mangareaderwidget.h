#ifndef MANGAREADERWIDGET_H
#define MANGAREADERWIDGET_H

#include <QWidget>
#include "mangainfo.h"

namespace Ui {
class MangaReaderWidget;
}

class MangaReaderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MangaReaderWidget(QWidget *parent = 0);
    ~MangaReaderWidget();

    void showImage(const QString &path);
    void updateReaderLabels(MangaInfo *currentmanga);


public slots:
    void addImageToCache(const QString &path);

signals:
    void changeView(int page);
    void advancPageClicked(bool direction);
    void closeApp();
    void back();

private slots:
    void on_pushButtonReaderHome_clicked();
    void on_pushButtonReaderBack_clicked();
    void on_pushButtonReaderClose_clicked();

    void on_mangaImageContainer_clicked(QPoint pos);
private:
    Ui::MangaReaderWidget *ui;

    void adjustSizes();

    QList<QPixmap *> imgcache;
    QList<QString> imgcachepaths;
};

#endif // MANGAREADERWIDGET_H
