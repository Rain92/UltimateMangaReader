#include "favoriteswidget.h"

#include "ui_favoriteswidget.h"

FavoritesWidget::FavoritesWidget(QWidget *parent)
    : QWidget(parent), favoritesmanager(nullptr), ui(new Ui::FavoritesWidget)
{
    ui->setupUi(this);

    adjustSizes();
}

FavoritesWidget::~FavoritesWidget() { delete ui; }

void FavoritesWidget::adjustSizes()
{
    ui->tableWidget->setHorizontalHeaderLabels(QStringList() << "Manga"
                                                             << "Host"
                                                             << "Status"
                                                             << "My Progress");
    ui->tableWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    QHeaderView *verticalHeader = ui->tableWidget->verticalHeader();

    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(favoritesectonheight);

    QHeaderView *horizontalHeader = ui->tableWidget->horizontalHeader();
    horizontalHeader->setSectionsClickable(false);

    for (int i = 0; i < 4; i++)
        horizontalHeader->setSectionResizeMode(i, QHeaderView::Stretch);

    ui->tableWidget->setVerticalScrollBar(
        new CScrollBar(Qt::Vertical, ui->tableWidget));
}

void FavoritesWidget::showFavoritesList()
{
    favoritesmanager->updateInfos();

    ui->tableWidget->clearContents();
    while (ui->tableWidget->model()->rowCount() > 0)
        ui->tableWidget->removeRow(0);

    for (int r = 0; r < favoritesmanager->favoriteinfos.count(); r++)
    {
        insertRow(favoritesmanager->favoriteinfos[r], r);
        QObject::connect(favoritesmanager->favoriteinfos[r].get(),
                         &MangaInfo::updatedSignal, this,
                         &FavoritesWidget::mangaUpdated);
        QObject::connect(favoritesmanager->favoriteinfos[r].get(),
                         &MangaInfo::coverLoaded, this,
                         &FavoritesWidget::coverLoaded);
    }

    ui->tableWidget->verticalScrollBar()->setValue(0);
}

void FavoritesWidget::insertRow(const QSharedPointer<MangaInfo> &fav, int row)
{
    QWidget *titlewidget =
        makeIconTextWidget(fav->coverThumbnailPath(), fav->title,
                           QSize(favoritecoverheight, favoritecoverheight));

    ui->tableWidget->insertRow(row);

    QTableWidgetItem *hostwidget = new QTableWidgetItem(fav->hostname);
    hostwidget->setTextAlignment(Qt::AlignCenter);

    ReadingProgress progress(fav->hostname, fav->title);

    QString statusstring = (fav->updated ? "Updated!\n" : fav->status + "\n") +
                           "Chapters: " + QString::number(progress.numChapters);
    QTableWidgetItem *chapters = new QTableWidgetItem(statusstring);
    chapters->setTextAlignment(Qt::AlignCenter);

    QString progressstring =
        "Chapter: " + QString::number(progress.index.chapter + 1) +
        "\nPage: " + QString::number(progress.index.page + 1);
    QTableWidgetItem *progressitem = new QTableWidgetItem(progressstring);
    progressitem->setTextAlignment(Qt::AlignCenter);

    ui->tableWidget->setCellWidget(row, 0, titlewidget);
    ui->tableWidget->setItem(row, 1, hostwidget);
    ui->tableWidget->setItem(row, 2, chapters);
    ui->tableWidget->setItem(row, 3, progressitem);
}

void FavoritesWidget::moveFavoriteToFront(int i)
{
    favoritesmanager->moveFavoriteToFront(i);

    ui->tableWidget->removeRow(i);
    insertRow(favoritesmanager->favoriteinfos.at(0), 0);

    emit(mangaListUpdated());
}

void FavoritesWidget::mangaUpdated(bool newchapters)
{
    if (!newchapters)
        return;

    MangaInfo *mi = static_cast<MangaInfo *>(sender());

    int i = 0;
    while (favoritesmanager->favoriteinfos.at(i)->title != mi->title &&
           favoritesmanager->favoriteinfos.at(i)->title != mi->title)
        i++;

    moveFavoriteToFront(i);
}

void FavoritesWidget::coverLoaded()
{
    MangaInfo *mi = static_cast<MangaInfo *>(sender());

    int i = 0;
    while (favoritesmanager->favoriteinfos.at(i)->title != mi->title &&
           favoritesmanager->favoriteinfos.at(i)->title != mi->title)
        i++;

    QWidget *titlewidget = makeIconTextWidget(
        favoritesmanager->favoriteinfos.at(i)->coverThumbnailPath(),
        favoritesmanager->favoriteinfos.at(i)->title,
        QSize(favoritecoverheight, favoritecoverheight));
    ui->tableWidget->setCellWidget(i, 0, titlewidget);
}

QWidget *FavoritesWidget::makeIconTextWidget(const QString &path,
                                             const QString &text,
                                             const QSize &iconsize)
{
    QWidget *widget = new QWidget();

    QLabel *textlabel = new QLabel(text, widget);

    QLabel *iconlabel = new QLabel(widget);
    iconlabel->setMaximumSize(iconsize);
    iconlabel->setScaledContents(false);
    iconlabel->setPixmap(QPixmap(path));

    QVBoxLayout *vlayout = new QVBoxLayout(widget);
    vlayout->setAlignment(Qt::AlignCenter);
    QHBoxLayout *hlayout = new QHBoxLayout();
    hlayout->addWidget(iconlabel);
    hlayout->setAlignment(Qt::AlignCenter);

    vlayout->addLayout(hlayout);
    vlayout->addWidget(textlabel);
    vlayout->setContentsMargins(0, 0, 0, 0);
    vlayout->setSpacing(2);
    vlayout->setMargin(0);
    widget->setLayout(vlayout);

    widget->setProperty("ptext", text);

    return widget;
}

void FavoritesWidget::on_tableWidget_cellClicked(int row, int column)
{
    moveFavoriteToFront(row);

    emit favoriteClicked(favoritesmanager->favoriteinfos.first(), column >= 2);
}
