#include "favoriteswidget.h"
#include "ui_favoriteswidget.h"
#include <qlabel.h>
#include <QFileInfo>
#include "configs.h"
#include "cscrollbar.h"


FavoritesWidget::FavoritesWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FavoritesWidget),
    favorites(nullptr)
{
    ui->setupUi(this);

    adjustSizes();
}

FavoritesWidget::~FavoritesWidget()
{
    delete ui;
}

void FavoritesWidget::adjustSizes()
{
    ui->tableWidget->setHorizontalHeaderLabels(QStringList() << "Manga" << "Host" << "Status" << "My Progress");
    ui->tableWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    QHeaderView *verticalHeader = ui->tableWidget->verticalHeader();
    verticalHeader->setResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(favoritesectonheight);

    QHeaderView *horizontalHeader = ui->tableWidget->horizontalHeader();
    horizontalHeader->setClickable(false);

    for (int i = 0; i < 4; i++)
        horizontalHeader->setResizeMode(i, QHeaderView::Stretch);

    ui->tableWidget->setVerticalScrollBar(new CScrollBar(Qt::Vertical, ui->tableWidget));
}


void FavoritesWidget::showFavoritesList(QList<Favorite> *favs)
{
    favorites = favs;

    while (infos.count() > 0)
    {
        delete infos.first();
        infos.removeFirst();
    }

    ui->tableWidget->clearContents();
    while (ui->tableWidget->model()->rowCount() > 0)
        ui->tableWidget->removeRow(0);
    int r =  0;
    foreach (const Favorite &fav, *favs)
    {
        insertRow(fav, r);
        r++;

        foreach (AbstractMangaSource *s, mangasources)
        {
            if (s->name != fav.hostname)
                continue;

            MangaInfo *mi = s->loadMangaInfo(fav.mangalink, fav.title);
            QObject::connect(mi, SIGNAL(updated()), this, SLOT(mangaUpdated()));
            infos.append(mi);
            break;
        }
    }
}

void FavoritesWidget::insertRow(const Favorite &fav, int row)
{
    QWidget *titlewidget = makeIconTextWidget(fav.coverpathscaled(), fav.title, QSize(favoritecoverheight, favoritecoverheight));

    ui->tableWidget->insertRow(row);

    QTableWidgetItem *hostwidget = new QTableWidgetItem(fav.hostname);
    hostwidget->setTextAlignment(Qt::AlignCenter);

    QString statusstring = (fav.updated ? "Updated!\n" : fav.status + "\n") + "Chapters: " + QString::number(fav.numchapters);
    QTableWidgetItem *chapters = new QTableWidgetItem(statusstring);
    chapters->setTextAlignment(Qt::AlignCenter);

    QString progressstring = "Chapter: " + QString::number(fav.currentindex.chapter + 1) + "\nPage: " + QString::number(fav.currentindex.page + 1);
    QTableWidgetItem *progress = new QTableWidgetItem(progressstring);
    progress->setTextAlignment(Qt::AlignCenter);

    ui->tableWidget->setCellWidget(row, 0, titlewidget);
    ui->tableWidget->setItem(row, 2, hostwidget);
    ui->tableWidget->setItem(row, 2, chapters);
    ui->tableWidget->setItem(row, 3, progress);
}


void FavoritesWidget::mangaUpdated()
{
    MangaInfo *mi = static_cast<MangaInfo *>(sender());

    int i = 0;
    while (favorites->at(i).title != mi->title && favorites->at(i).title != mi->title)
        i++;

    favorites->move(i, 0);
    favorites->first().updated = true;
    favorites->first().numchapters = mi->numchapters;

    ui->tableWidget->removeRow(i);
    insertRow(favorites->at(0), 0);

    emit(mangaListUpdated());
}

QWidget *FavoritesWidget::makeIconTextWidget(const QString &path, const QString &text, const QSize &iconsize)
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

    return widget;
}

void FavoritesWidget::on_tableWidget_cellClicked(int row, int column)
{
    emit favoriteClicked(favorites->at(row), column >= 2);
}
