#ifndef FAVORITESWIDGET_H
#define FAVORITESWIDGET_H

#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QWidget>

#include "favoritesmanager.h"
#include "imageprocessingnative.h"
#include "readingprogress.h"
#include "sizes.h"

namespace Ui
{
class FavoritesWidget;
}

// A cover thumbnail to load, or to make from the cover if there is none yet.
struct ThumbnailRequest
{
    QString key;
    QString coverPath;
    QString thumbnailPath;
    qreal pixelRatio;
};

class FavoritesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FavoritesWidget(QWidget *parent = nullptr);
    ~FavoritesWidget();

    // Shows the list right away, the thumbnails and the updates of the infos (downloads) follow.
    void showFavoritesList();
    FavoritesManager *favoritesManager;

signals:
    void favoriteClicked(QSharedPointer<MangaInfo> fav, bool current);
    void error(const QString &error);

private slots:
    void on_tableWidget_cellClicked(int row, int column);
    void mangaUpdated(bool updated);
    void coverLoaded();

private:
    Ui::FavoritesWidget *ui;

    // the thumbnails are loaded in batches on worker threads
    QFutureWatcher<QImage> thumbnailWatcher;
    QList<ThumbnailRequest> thumbnailBatch;
    QList<ThumbnailRequest> queuedThumbnails;

    // a click that has to download the info runs an event loop
    bool opening;

    void rebuildTable();
    void insertRow(int row);
    void updateStatus(int row);
    void connectInfo(const QSharedPointer<MangaInfo> &info);
    void adjustUI();
    void moveFavoriteToFront(int i);

    void loadThumbnails(const QList<int> &rows);
    void startThumbnailBatch();
    void thumbnailReady(int index);
    void setThumbnail(int row, const QImage &image);
    QLabel *iconLabel(int row) const;
    QString rowKey(int row) const;
    bool rowMatches(int row) const;
    int findRow(const QString &key) const;

    QWidget *makeIconTextWidget(const QString &key, const QString &text);
};

#endif  // FAVORITESWIDGET_H
