#include "favoriteswidget.h"

#include <QSaveFile>

#include "ui_favoriteswidget.h"

static QString favoriteKey(const Favorite &fav)
{
    return fav.hostname + '/' + fav.title;
}

static QPixmap labelPixmap(const QLabel *label)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return label->pixmap();
#else
    return label->pixmap(Qt::ReturnByValue);
#endif
}

// Runs on a worker thread.
static QImage loadThumbnail(const ThumbnailRequest &request)
{
    if (QFile::exists(request.thumbnailPath))
    {
        QImage image = loadQImageFast(request.thumbnailPath);
        if (!image.isNull())
            return image;
    }

    // the cover is still being downloaded, its coverLoaded() signal comes later
    if (request.coverPath.isEmpty() || !QFile::exists(request.coverPath))
        return QImage();

    QImage image = loadQImageFast(request.coverPath);
    if (image.isNull())
        return image;

    int size = qRound(SIZES.favoriteCoverSize * request.pixelRatio);
    image = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // the list might load it again while it is written
    QSaveFile file(request.thumbnailPath);
    const char *format = request.thumbnailPath.endsWith(".png") ? "PNG" : "JPG";
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, format) || !file.commit())
        qDebug() << "Failed to save thumbnail:" << request.thumbnailPath;

    return image;
}

FavoritesWidget::FavoritesWidget(QWidget *parent)
    : QWidget(parent),
      favoritesManager(nullptr),
      ui(new Ui::FavoritesWidget),
      thumbnailWatcher(),
      thumbnailBatch(),
      queuedThumbnails(),
      opening(false)
{
    ui->setupUi(this);

    adjustUI();

    connect(&thumbnailWatcher, &QFutureWatcher<QImage>::resultReadyAt, this, &FavoritesWidget::thumbnailReady);
    connect(&thumbnailWatcher, &QFutureWatcher<QImage>::finished, this, &FavoritesWidget::startThumbnailBatch);
}

FavoritesWidget::~FavoritesWidget()
{
    // the batch keeps running until it is done, without reporting back
    thumbnailWatcher.disconnect(this);
    delete ui;
}

void FavoritesWidget::adjustUI()
{
    ui->tableWidget->setHorizontalHeaderLabels(QStringList() << "Manga"
                                                             << "Host"
                                                             << "Status"
                                                             << "My Progress");
    ui->tableWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    activateScroller(ui->tableWidget);

    QHeaderView *verticalHeader = ui->tableWidget->verticalHeader();

    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(SIZES.favoriteSectonHeight);

    QHeaderView *horizontalHeader = ui->tableWidget->horizontalHeader();
    horizontalHeader->setSectionsClickable(false);
    horizontalHeader->setFrameShape(QFrame::Box);
    horizontalHeader->setLineWidth(1);

    for (int i = 0; i < 4; i++)
        horizontalHeader->setSectionResizeMode(i, QHeaderView::Stretch);
}

void FavoritesWidget::showFavoritesList()
{
    // only from the cache, no downloads
    favoritesManager->loadInfos();

    const auto &favorites = favoritesManager->favorites;

    bool rebuild = ui->tableWidget->rowCount() != favorites.count();
    for (int i = 0; !rebuild && i < favorites.count(); i++)
        rebuild = rowKey(i) != favoriteKey(favorites[i]);

    if (rebuild)
    {
        rebuildTable();
    }
    else
    {
        for (int i = 0; i < ui->tableWidget->rowCount(); i++)
        {
            updateStatus(i);
            // a click might have replaced it
            connectInfo(favoritesManager->favoriteinfos.value(i));
        }
    }

    // after the list is on the screen
    QTimer::singleShot(0, this,
                       [this]()
                       {
                           QList<int> rows;
                           for (int i = 0; i < ui->tableWidget->rowCount(); i++)
                           {
                               auto label = iconLabel(i);
                               if (label && labelPixmap(label).isNull())
                                   rows.append(i);
                           }
                           loadThumbnails(rows);

                           favoritesManager->updateInfos();
                       });
}

void FavoritesWidget::rebuildTable()
{
    ui->tableWidget->setRowCount(0);

    for (int i = 0; i < favoritesManager->favorites.count(); i++)
    {
        insertRow(i);
        connectInfo(favoritesManager->favoriteinfos.value(i));
    }
}

void FavoritesWidget::connectInfo(const QSharedPointer<MangaInfo> &info)
{
    if (!info)
        return;

    connect(info.get(), &MangaInfo::updatedSignal, this, &FavoritesWidget::mangaUpdated, Qt::UniqueConnection);
    connect(info.get(), &MangaInfo::coverLoaded, this, &FavoritesWidget::coverLoaded, Qt::UniqueConnection);
}

static QString statusText(const QSharedPointer<MangaInfo> &info)
{
    if (!info)
        return "Source not enabled";

    if (info->chapters.isEmpty())
        return "Not loaded yet";

    return (info->updated ? "New chapters!\n" : info->status + "\n") +
           "Chapters: " + QString::number(info->chapters.size());
}

static QString progressText(const Favorite &fav)
{
    ReadingProgress progress(fav.hostname, fav.title);

    return "Chapter: " + QString::number(progress.index.chapter + 1) +
           "\nPage: " + QString::number(progress.index.page + 1);
}

void FavoritesWidget::updateStatus(int row)
{
    auto status = ui->tableWidget->item(row, 2);
    auto progress = ui->tableWidget->item(row, 3);
    if (!status || !progress)
        return;

    status->setText(statusText(favoritesManager->favoriteinfos.value(row)));
    progress->setText(progressText(favoritesManager->favorites[row]));
}

void FavoritesWidget::insertRow(int row)
{
    const auto &fav = favoritesManager->favorites[row];

    QWidget *titlewidget = makeIconTextWidget(favoriteKey(fav), fav.title);

    ui->tableWidget->insertRow(row);

    QTableWidgetItem *hostwidget = new QTableWidgetItem(fav.hostname);
    hostwidget->setTextAlignment(Qt::AlignCenter);

    QTableWidgetItem *chapters = new QTableWidgetItem(statusText(favoritesManager->favoriteinfos.value(row)));
    chapters->setTextAlignment(Qt::AlignCenter);

    QTableWidgetItem *progressitem = new QTableWidgetItem(progressText(fav));
    progressitem->setTextAlignment(Qt::AlignCenter);

    ui->tableWidget->setCellWidget(row, 0, titlewidget);
    ui->tableWidget->setItem(row, 1, hostwidget);
    ui->tableWidget->setItem(row, 2, chapters);
    ui->tableWidget->setItem(row, 3, progressitem);
}

void FavoritesWidget::moveFavoriteToFront(int i)
{
    if (i <= 0 || i >= ui->tableWidget->rowCount())
        return;

    auto label = iconLabel(i);
    QPixmap thumbnail = label ? labelPixmap(label) : QPixmap();

    favoritesManager->moveFavoriteToFront(i);

    ui->tableWidget->removeRow(i);
    insertRow(0);
    if (!thumbnail.isNull())
        setThumbnail(0, thumbnail.toImage());

    ui->tableWidget->scrollToTop();
}

void FavoritesWidget::mangaUpdated(bool updated)
{
    int i = favoritesManager->findFavorite(qobject_cast<MangaInfo *>(sender()));
    if (!rowMatches(i))
        return;

    updateStatus(i);

    if (updated)
        moveFavoriteToFront(i);
}

void FavoritesWidget::coverLoaded()
{
    int i = favoritesManager->findFavorite(qobject_cast<MangaInfo *>(sender()));
    if (!rowMatches(i))
        return;

    loadThumbnails({i});
}

void FavoritesWidget::loadThumbnails(const QList<int> &rows)
{
    for (int row : rows)
    {
        auto info = favoritesManager->favoriteinfos.value(row);
        if (!info)
            continue;

        ThumbnailRequest request{rowKey(row), info->coverPath, info->coverThumbnailPath(), windowPixelRatio()};
        if (request.thumbnailPath.isEmpty())
            continue;

        auto sameKey = [&request](const ThumbnailRequest &r) { return r.key == request.key; };
        if (std::none_of(queuedThumbnails.begin(), queuedThumbnails.end(), sameKey))
            queuedThumbnails.append(request);
    }

    startThumbnailBatch();
}

void FavoritesWidget::startThumbnailBatch()
{
    if (thumbnailWatcher.isRunning() || queuedThumbnails.isEmpty())
        return;

    thumbnailBatch = queuedThumbnails;
    queuedThumbnails.clear();
    thumbnailWatcher.setFuture(QtConcurrent::mapped(thumbnailBatch, loadThumbnail));
}

void FavoritesWidget::thumbnailReady(int index)
{
    if (index < 0 || index >= thumbnailBatch.size())
        return;

    QImage image = thumbnailWatcher.resultAt(index);
    // the rows might have moved since the batch started
    int row = findRow(thumbnailBatch[index].key);
    if (row >= 0 && !image.isNull())
        setThumbnail(row, image);
}

void FavoritesWidget::setThumbnail(int row, const QImage &image)
{
    auto label = iconLabel(row);
    if (!label)
        return;

    // thumbnails of an older version might have been made for another pixel ratio
    QSize size = image.size().scaled(SIZES.favoriteCoverSize, SIZES.favoriteCoverSize, Qt::KeepAspectRatio);
    label->setFixedSize(size);
    label->setPixmap(QPixmap::fromImage(image));
}

QLabel *FavoritesWidget::iconLabel(int row) const
{
    auto widget = ui->tableWidget->cellWidget(row, 0);
    return widget ? widget->findChild<QLabel *>("icon") : nullptr;
}

QString FavoritesWidget::rowKey(int row) const
{
    auto widget = ui->tableWidget->cellWidget(row, 0);
    return widget ? widget->property("favoriteKey").toString() : QString();
}

// The favorites can change while the list isn't shown (e.g. a favorite is added in the info widget), the
// list is rebuilt when it is shown again.
bool FavoritesWidget::rowMatches(int row) const
{
    return row >= 0 && row < ui->tableWidget->rowCount() && row < favoritesManager->favorites.count() &&
           rowKey(row) == favoriteKey(favoritesManager->favorites[row]);
}

int FavoritesWidget::findRow(const QString &key) const
{
    for (int i = 0; i < ui->tableWidget->rowCount(); i++)
        if (rowKey(i) == key)
            return i;

    return -1;
}

QWidget *FavoritesWidget::makeIconTextWidget(const QString &key, const QString &text)
{
    QWidget *widget = new QWidget();

    QLabel *textlabel = new QLabel(text, widget);

    // the thumbnail is loaded later, keep its space
    QLabel *iconlabel = new QLabel(widget);
    iconlabel->setObjectName("icon");
    iconlabel->setScaledContents(true);
    iconlabel->setFixedSize(SIZES.favoriteCoverSize * 7 / 10, SIZES.favoriteCoverSize);

    QVBoxLayout *vlayout = new QVBoxLayout(widget);
    vlayout->setAlignment(Qt::AlignCenter);
    QHBoxLayout *hlayout = new QHBoxLayout();
    hlayout->addWidget(iconlabel);
    hlayout->setAlignment(Qt::AlignCenter);

    vlayout->addLayout(hlayout);
    vlayout->addWidget(textlabel);
    vlayout->setContentsMargins(0, 0, 0, 0);
    vlayout->setSpacing(2);
    widget->setLayout(vlayout);

    widget->setProperty("favoriteKey", key);

    return widget;
}

void FavoritesWidget::on_tableWidget_cellClicked(int row, int column)
{
    if (opening)
        return;

    if (!rowMatches(row))
    {
        showFavoritesList();
        return;
    }

    if (!favoritesManager->favoriteinfos.value(row))
    {
        emit error("The source of this manga is not enabled.");
        return;
    }

    qDebug() << "clicked fav" << row << favoritesManager->favorites[row].title;

    // downloads the info if it isn't cached yet, the list can change meanwhile
    opening = true;
    auto info = favoritesManager->requireInfo(row);
    opening = false;

    if (!info)
        return;

    connectInfo(info);
    int i = favoritesManager->findFavorite(info.get());
    if (rowMatches(i))
        moveFavoriteToFront(i);
    else
        favoritesManager->moveFavoriteToFront(i);

    emit favoriteClicked(info, column >= 2);
}
