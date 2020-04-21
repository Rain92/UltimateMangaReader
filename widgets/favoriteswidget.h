#ifndef FAVORITESWIDGET_H
#define FAVORITESWIDGET_H

#include <QFileInfo>
#include <QLabel>
#include <QWidget>

#include "favoritesmanager.h"
#include "readingprogress.h"
#include "sizes.h"

namespace Ui
{
class FavoritesWidget;
}

class FavoritesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FavoritesWidget(QWidget *parent = nullptr);
    ~FavoritesWidget();

    void showFavoritesList();
    FavoritesManager *favoritesmanager;

signals:
    void favoriteClicked(QSharedPointer<MangaInfo> fav, bool current);

private slots:
    void on_tableWidget_cellClicked(int row, int column);
    void mangaUpdated(bool newchapters);
    void coverLoaded();

private:
    Ui::FavoritesWidget *ui;

    void insertRow(const QSharedPointer<MangaInfo> &fav, int row);
    void adjustUI();
    void moveFavoriteToFront(int i);

    QWidget *makeIconTextWidget(const QString &path, const QString &text, const QSize &iconsize);
};

#endif  // FAVORITESWIDGET_H
