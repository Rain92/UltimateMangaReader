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
//    ui->tableWidget->setIconSize(QSize(100, 80));


    QHeaderView *verticalHeader = ui->tableWidget->verticalHeader();
    verticalHeader->setResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(favoritesectonheight);

    QHeaderView *horizontalHeader = ui->tableWidget->horizontalHeader();
//    horizontalHeader->setResizeMode(QHeaderView::Fixed);
    horizontalHeader->setClickable(false);
    for (int i = 0; i < 4; i++)
        horizontalHeader->setResizeMode(i, QHeaderView::Stretch);
//    horizontalHeader->setResizeMode(1, QHeaderView::Fixed);
//    horizontalHeader->setMinimumSectionSize(favoritesectionwidth);
//    for (int i = 1; i < 4; i++)
//        ui->tableWidget->setColumnWidth(i, this->width() / 6);

    ui->tableWidget->setVerticalScrollBar(new CScrollBar(Qt::Vertical, ui->tableWidget));
//    resizeEvent(nullptr);

}


void FavoritesWidget::updateList(const QList<ReadingState> &favs)
{
    readingstates = favs;

    ui->tableWidget->clearContents();
    int r =  0;
    foreach (const ReadingState &fav, favs)
    {
        QString coverpath = downloaddircovers + "/" + makePathLegal(fav.hostname + "_" + fav.title);

        if (QFileInfo(coverpath + ".jpg").exists())
            coverpath += ".jpg";
        else if (QFileInfo(coverpath + ".png").exists())
            coverpath += ".png";

        QString hosticonpath = ":/resources/images/icons/" + fav.hostname.toLower() + ".png";

        QWidget *titlewidget = makeIconTextWidget(coverpath, fav.title, QSize(favoritecoverheight, favoritecoverheight));
        QWidget *hostwidget = makeIconTextWidget(hosticonpath, fav.hostname, QSize(favoritecoverheight, favoritecoverheight));

        ui->tableWidget->insertRow(r);
        QTableWidgetItem *chapters = new QTableWidgetItem(QString::number(fav.oldnumchapters));
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

    QLabel *textlabel = new QLabel(text);
    textlabel->setAlignment(Qt::AlignCenter);

    QLabel *iconlabel = new QLabel();
    iconlabel->setAlignment(Qt::AlignCenter);
    iconlabel->setMaximumSize(iconsize);
    iconlabel->setScaledContents(true);
    iconlabel->setPixmap(QPixmap(path));

    QVBoxLayout *vlayout = new QVBoxLayout(widget);
    vlayout->addWidget(iconlabel);
    vlayout->addWidget(textlabel);
    vlayout->setAlignment(Qt::AlignCenter);
    vlayout->setContentsMargins(0, 0, 0, 0);
    vlayout->setSpacing(2);
    vlayout->setMargin(0);
    widget->setLayout(vlayout);

    return widget;
}

void FavoritesWidget::on_tableWidget_cellClicked(int row, int column)
{
    emit favoriteClicked(readingstates[row], column != 0);
}
