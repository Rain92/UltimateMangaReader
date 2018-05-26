#include "favoriteswidget.h"
#include "ui_favoriteswidget.h"
#include <qlabel.h>
#include <QFileInfo>
#include "configs.h"
#include "cscrollbar.h"


FavoritesWidget::FavoritesWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FavoritesWidget),
    readingstates()
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
    ui->tableWidget->setHorizontalHeaderLabels(QStringList() << "Manga" << "Host" << "Chapters" << "Progress");

    QHeaderView *verticalHeader = ui->tableWidget->verticalHeader();
    verticalHeader->setResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(favoritesectonheight);

    QHeaderView *horizontalHeader = ui->tableWidget->horizontalHeader();
    horizontalHeader->setClickable(false);

    for (int i = 0; i < 4; i++)
        horizontalHeader->setResizeMode(i, QHeaderView::Stretch);

    ui->tableWidget->setVerticalScrollBar(new CScrollBar(Qt::Vertical, ui->tableWidget));
}


void FavoritesWidget::updateList(const QList<Favorite> &favs)
{
    readingstates = favs;

    ui->tableWidget->clearContents();
    while (ui->tableWidget->model()->rowCount() > 0)
        ui->tableWidget->removeRow(0);
    int r =  0;
    foreach (const Favorite &fav, favs)
    {
        QString coverpath = mangainfodir(fav.hostname, fav.title) + "cover";

        if (QFileInfo(coverpath + ".jpg").exists())
            coverpath += ".jpg";
        else if (QFileInfo(coverpath + ".png").exists())
            coverpath += ".png";

        QString hosticonpath = ":/resources/images/icons/" + fav.hostname.toLower() + ".png";

        QWidget *titlewidget = makeIconTextWidget(coverpath, fav.title, QSize(favoritecoverheight, favoritecoverheight));
        QWidget *hostwidget = makeIconTextWidget(hosticonpath, fav.hostname, QSize(favoritecoverheight, favoritecoverheight));

        ui->tableWidget->insertRow(r);
        QTableWidgetItem *chapters = new QTableWidgetItem(QString::number(fav.numchapters));
        chapters->setTextAlignment(Qt::AlignCenter);
        QTableWidgetItem *progress = new QTableWidgetItem(QString::number(fav.currentindex.chapter));
        progress->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setCellWidget(r, 0, titlewidget);
        ui->tableWidget->setCellWidget(r, 1, hostwidget);
        ui->tableWidget->setItem(r, 2, chapters);
        ui->tableWidget->setItem(r, 3, progress);
        r++;
    }
}


QWidget *FavoritesWidget::makeIconTextWidget(const QString &path, const QString &text, const QSize &iconsize)
{
    QWidget *widget = new QWidget();

    QLabel *textlabel = new QLabel(text, widget);

    QLabel *iconlabel = new QLabel(widget);
    iconlabel->setMaximumSize(iconsize);
    iconlabel->setScaledContents(true);
    iconlabel->setPixmap(QPixmap(path));

    QVBoxLayout *vlayout = new QVBoxLayout(widget);
    vlayout->setAlignment(Qt::AlignCenter);
    QHBoxLayout *hlayout = new QHBoxLayout();
    hlayout->addWidget(iconlabel);

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
    emit favoriteClicked(readingstates[row], column >= 2);
}
