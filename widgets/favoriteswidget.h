#ifndef FAVORITESWIDGET_H
#define FAVORITESWIDGET_H

#include <QWidget>
#include "favoritesmanager.h"

namespace Ui {
class FavoritesWidget;
}

class FavoritesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FavoritesWidget(QWidget *parent = 0);
    ~FavoritesWidget();

    void showFavoritesList(QList<Favorite> *favs);


    QList<AbstractMangaSource *> mangasources;
signals:
    void favoriteClicked(QSharedPointer<MangaInfo> fav, bool current);
    void mangaListUpdated();

private slots:
    void on_tableWidget_cellClicked(int row, int column);
    void mangaUpdated();

private:
    Ui::FavoritesWidget *ui;
    QList<Favorite> *favorites;

    void insertRow(const Favorite &fav, int row);
    void adjustSizes();

    QList< QSharedPointer<MangaInfo> > infos;

    QWidget *makeIconTextWidget(const QString &path, const QString &text, const QSize &iconsize);

};

#endif // FAVORITESWIDGET_H
