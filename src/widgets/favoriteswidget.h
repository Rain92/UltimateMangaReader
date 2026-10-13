#pragma once

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

    void setFavoritesManager(FavoritesManager *manager);
    // Shows the list right away, the thumbnails and the updates of the infos (downloads) follow.
    void showFavoritesList();

signals:
    void favoriteClicked(QSharedPointer<MangaInfo> fav, bool current);
    void error(const QString &error);

private slots:
    void on_tableWidget_cellClicked(int row, int column);

private:
    FavoritesManager *favoritesManager;
    Ui::FavoritesWidget *ui;

    // the table is built when it's shown the first time, then it follows the favorites
    bool built;
    // a click that has to download the info runs an event loop
    bool opening;

    void rebuildTable();
    void insertRow(int row);
    void moveRow(int from, int to);
    void updateStatus(int row);
    void adjustUI();

    void setThumbnail(int row, const QImage &image);
    QLabel *iconLabel(int row) const;
    QString rowKey(int row) const;
    int findRow(const QString &key) const;

    QWidget *makeIconTextWidget(const QString &key, const QString &text);
};
