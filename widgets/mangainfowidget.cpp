#include "mangainfowidget.h"
#include "ui_mangainfowidget.h"
#include "cscrollbar.h"

#include "qstringlistmodel.h"
#include <QScrollBar>
#include <QResizeEvent>

MangaInfoWidget::MangaInfoWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MangaInfoWidget),
    currentmanga(nullptr)
{
    ui->setupUi(this);
    adjustSizes();
}

MangaInfoWidget::~MangaInfoWidget()
{
    delete ui;
}

void  MangaInfoWidget::adjustSizes()
{
    ui->pushButtonReadContinue->setMinimumHeight(buttonsize);
    ui->pushButtonReadFirst->setMinimumHeight(buttonsize);
    ui->pushButtonReadLatest->setMinimumHeight(buttonsize);
    ui->pushButtonMangaInfoAddFavorites->setMinimumHeight(buttonsizeaddfavorite);
    ui->pushButtonMangaInfoAddFavorites->setMaximumWidth(buttonsizeaddfavorite);


    ui->listViewChapters->setVerticalScrollBar(new CScrollBar(Qt::Vertical, ui->listViewChapters));
    ui->listViewChapters->setHorizontalScrollBar(new CScrollBar(Qt::Horizontal, ui->listViewChapters));
    ui->listViewChapters->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->listViewChapters->setUniformItemSizes(true);

    ui->scrollAreaMangaInfoSummary->setVerticalScrollBar(new CScrollBar(Qt::Vertical, ui->scrollAreaMangaInfoSummary, summaryscrollbarwidth));

    QFont titlefont = ui->labelMangaInfoTitle->font();
    titlefont.setPointSize(titlefont.pointSize() * 2);
    ui->labelMangaInfoTitle->setFont(titlefont);
//    ui->labelMangaInfoCover->setMinimumHeight(coversize);
//    ui->labelMangaInfoCover->setMaximumWidth(coversize);
//    resizeEvent(nullptr);
}

void MangaInfoWidget::setManga(MangaInfo *manga)
{
    currentmanga = manga;

    QStringListModel *model = new QStringListModel(this);
    model->setStringList(currentmanga->chapertitlesreversed);

    ui->listViewChapters->setModel(model);

    ui->labelMangaInfoTitle->setText(currentmanga->title);

    ui->labelMangaInfoLabelAuthor->setText(currentmanga->author);
    ui->labelMangaInfoLabelArtist->setText(currentmanga->artist);
    ui->labelMangaInfoLabelGenres->setText(currentmanga->genres);
    ui->labelMangaInfoLabelStaus->setText(currentmanga->status);
    ui->labelMangaInfoLabelYear->setText(currentmanga->releaseyear);
    ui->labelMangaInfoLabelSummary->setText(currentmanga->summary);

    QPixmap img;
//    qDebug() << currentmanga->coverpath;
    img.load(currentmanga->coverpath);
    ui->labelMangaInfoCover->setPixmap(img.scaled(coversize, coversize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QObject::connect(currentmanga, SIGNAL(updated()), this, SLOT(updateManga()));
}

void MangaInfoWidget::updateManga()
{
    qDebug() << "update";
    ui->labelMangaInfoLabelStaus->setText(currentmanga->status);
    static_cast<QStringListModel *>(ui->listViewChapters->model())->setStringList(currentmanga->chapertitlesreversed);
}


void MangaInfoWidget::setFavoriteButtonState(bool state)
{
    if (state)
        ui->pushButtonMangaInfoAddFavorites->setText("+");
    else
        ui->pushButtonMangaInfoAddFavorites->setText("-");
}

void MangaInfoWidget::on_pushButtonMangaInfoAddFavorites_clicked()
{
    if (currentmanga != nullptr)
        emit toggleFavoriteClicked(currentmanga);
}


void MangaInfoWidget::on_listViewChapters_clicked(const QModelIndex &index)
{
    emit readMangaClicked(MangaIndex(currentmanga->numchapters - 1 - index.row(), 0));
}

void MangaInfoWidget::on_pushButtonReadLatest_clicked()
{
    emit readMangaClicked(MangaIndex(currentmanga->numchapters - 1, 0));
}

void MangaInfoWidget::on_pushButtonReadContinue_clicked()
{
    emit readMangaClicked(currentmanga->currentindex);
}

void MangaInfoWidget::on_pushButtonReadFirst_clicked()
{
    emit readMangaClicked(MangaIndex(0, 0));
}
