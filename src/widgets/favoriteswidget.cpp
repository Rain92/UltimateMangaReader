#include "favoriteswidget.h"

#include "perftimer.h"
#include "ui_favoriteswidget.h"
#include "widgetutils.h"

FavoritesWidget::FavoritesWidget(QWidget *parent)
    : QWidget(parent), favoritesManager(nullptr), ui(new Ui::FavoritesWidget), built(false), opening(false)
{
    ui->setupUi(this);

    adjustUI();
}

FavoritesWidget::~FavoritesWidget()
{
    delete ui;
}

void FavoritesWidget::setFavoritesManager(FavoritesManager *manager)
{
    favoritesManager = manager;

    // the table follows the favorites once it was built
    connect(manager, &FavoritesManager::favoritesReset, this,
            [this]()
            {
                if (built)
                    rebuildTable();
            });
    connect(manager, &FavoritesManager::favoriteInserted, this,
            [this](int i)
            {
                if (!built)
                    return;
                insertRow(i);
                favoritesManager->loadThumbnails({i}, windowPixelRatio());
            });
    connect(manager, &FavoritesManager::favoriteRemoved, this,
            [this](int i)
            {
                if (built)
                    ui->tableWidget->removeRow(i);
            });
    connect(manager, &FavoritesManager::favoriteMoved, this, &FavoritesWidget::moveRow);
    connect(manager, &FavoritesManager::favoriteChanged, this,
            [this](int i)
            {
                if (built)
                    updateStatus(i);
            });
    connect(manager, &FavoritesManager::thumbnailLoaded, this,
            [this](const QString &key, const QImage &image)
            {
                int row = findRow(key);
                if (row >= 0)
                    setThumbnail(row, image);
            });
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
    PerfTimer timer("showFavoritesList");

    // only from the cache, no downloads
    favoritesManager->loadInfos();

    if (!built)
    {
        built = true;
        rebuildTable();
    }

    // after the list is on the screen
    QTimer::singleShot(0, this,
                       [this]()
                       {
                           QList<int> rows;
                           for (int i = 0; i < ui->tableWidget->rowCount(); i++)
                           {
                               auto label = iconLabel(i);
                               if (label && label->pixmap().isNull())
                                   rows.append(i);
                           }
                           favoritesManager->loadThumbnails(rows, windowPixelRatio());

                           favoritesManager->updateInfos();
                       });
}

void FavoritesWidget::rebuildTable()
{
    ui->tableWidget->setRowCount(0);

    for (int i = 0; i < favoritesManager->count(); i++)
        insertRow(i);
}

static QString statusText(const QSharedPointer<MangaInfo> &info)
{
    if (!info)
        return "Source not enabled";

    // e.g. a MangaDex title whose chapters are all external links has an info but nothing to read
    if (info->chapters.isEmpty())
        return info->summary.isEmpty() && info->author.isEmpty() ? "Not loaded yet" : "No chapters available";

    return (info->updated ? "New chapters!\n" : info->status + "\n") +
           "Chapters: " + QString::number(info->chapters.size());
}

static QString progressText(const ReadingProgress &progress)
{
    return "Chapter: " + QString::number(progress.index.chapter + 1) +
           "\nPage: " + QString::number(progress.index.page + 1);
}

void FavoritesWidget::updateStatus(int row)
{
    auto status = ui->tableWidget->item(row, 2);
    auto progress = ui->tableWidget->item(row, 3);
    if (!status || !progress)
        return;

    status->setText(statusText(favoritesManager->info(row)));
    progress->setText(progressText(favoritesManager->progress(row)));
}

void FavoritesWidget::insertRow(int row)
{
    const auto &fav = favoritesManager->favorite(row);

    QWidget *titlewidget = makeIconTextWidget(fav.key(), fav.title);

    ui->tableWidget->insertRow(row);

    QTableWidgetItem *hostwidget = new QTableWidgetItem(fav.hostname);
    hostwidget->setTextAlignment(Qt::AlignCenter);

    QTableWidgetItem *chapters = new QTableWidgetItem(statusText(favoritesManager->info(row)));
    chapters->setTextAlignment(Qt::AlignCenter);

    QTableWidgetItem *progressitem = new QTableWidgetItem(progressText(favoritesManager->progress(row)));
    progressitem->setTextAlignment(Qt::AlignCenter);

    ui->tableWidget->setCellWidget(row, 0, titlewidget);
    ui->tableWidget->setItem(row, 1, hostwidget);
    ui->tableWidget->setItem(row, 2, chapters);
    ui->tableWidget->setItem(row, 3, progressitem);
}

void FavoritesWidget::moveRow(int from, int to)
{
    if (!built)
        return;

    auto label = iconLabel(from);
    QPixmap thumbnail = label ? label->pixmap() : QPixmap();

    ui->tableWidget->removeRow(from);
    insertRow(to);
    if (!thumbnail.isNull())
        setThumbnail(to, thumbnail.toImage());

    if (to == 0)
        ui->tableWidget->scrollToTop();
}

void FavoritesWidget::setThumbnail(int row, const QImage &image)
{
    auto label = iconLabel(row);
    if (!label)
        return;

    // the thumbnail might have been made for another pixel ratio (desktop: the window was on another screen)
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
    if (opening || row < 0 || row >= favoritesManager->count())
        return;

    if (!favoritesManager->info(row))
    {
        emit error("The source of this manga is not enabled.");
        return;
    }

    qDebug() << "clicked fav" << row << favoritesManager->favorite(row).title;

    // downloads the info if it isn't cached yet, the list can change meanwhile
    opening = true;
    auto info = favoritesManager->requireInfo(row);
    opening = false;

    if (!info)
        return;

    favoritesManager->moveFavoriteToFront(favoritesManager->indexOf(info.get()));

    emit favoriteClicked(info, column >= 2);
}
