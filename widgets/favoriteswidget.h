#ifndef FAVORITESWIDGET_H
#define FAVORITESWIDGET_H

#include <QWidget>
#include "ReadingStateManager.h"

namespace Ui {
class FavoritesWidget;
}

class FavoritesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FavoritesWidget(QWidget *parent = 0);
    ~FavoritesWidget();

    void updateList(const QList<ReadingState> &favs);

signals:
    void favoriteClicked(ReadingState fav, bool current);

private slots:
    void on_tableWidget_cellClicked(int row, int column);

private:
    Ui::FavoritesWidget *ui;
    QList<ReadingState> readingstates;

    void adjustSizes();

    QWidget *makeIconTextWidget(const QString &path, const QString &text, const QSize &iconsize);

};

#endif // FAVORITESWIDGET_H
