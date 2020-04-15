#include "mangainfowidget.h"

#include <QResizeEvent>
#include <QScrollBar>

#include "qstringlistmodel.h"
#include "ui_mangainfowidget.h"

MangaInfoWidget::MangaInfoWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::MangaInfoWidget), currentmanga()
{
    ui->setupUi(this);
    adjustSizes();
}

MangaInfoWidget::~MangaInfoWidget()
{
    delete ui;
}

void MangaInfoWidget::adjustSizes()
{
    ui->pushButtonReadContinue->setFixedHeight(buttonsize);
    ui->pushButtonReadFirst->setFixedHeight(buttonsize);
    ui->pushButtonReadLatest->setFixedHeight(buttonsize);

    ui->toolButtonAddFavorites->setFixedSize(buttonsizeaddfavorite, buttonsizeaddfavorite);
    ui->toolButtonAddFavorites->setIconSize(QSize(buttonsizeaddfavorite * 0.8, buttonsizeaddfavorite * 0.8));
    ui->toolButtonAddFavorites->setFocusPolicy(Qt::NoFocus);

    isfavoriteicon = QIcon(QPixmap(":/images/icons/favourite-star-full.png")
                               .scaledToHeight(buttonsizeaddfavorite, Qt::SmoothTransformation));
    isnotfavoriteicon = QIcon(QPixmap(":/images/icons/favourite-star.png")
                                  .scaledToHeight(buttonsizeaddfavorite, Qt::SmoothTransformation));

    ui->listViewChapters->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->listViewChapters->setUniformItemSizes(true);

    ui->labelMangaInfoTitle->setStyleSheet("font-size: 18pt");

    activateScroller(ui->scrollAreaMangaInfoSummary);
    activateScroller(ui->listViewChapters);
}

inline void updateLabel(QLabel *caption, QLabel *content, const QString &text)
{
    bool hide = text.length() <= 1;
    caption->setHidden(hide);
    content->setHidden(hide);

    content->setText(text);
}

void MangaInfoWidget::setManga(QSharedPointer<MangaInfo> manga)
{
    if (currentmanga != manga)
    {
        currentmanga.clear();
        currentmanga = manga;

        QObject::connect(currentmanga.get(), &MangaInfo::updatedSignal, this, &MangaInfoWidget::updateManga);

        QObject::connect(currentmanga.get(), &MangaInfo::coverLoaded, this, &MangaInfoWidget::updateCover);
    }

    updateInfos();
    updateCover();
}

void MangaInfoWidget::updateManga(bool newchapters)
{
    Q_UNUSED(newchapters)

    qDebug() << "updated" << currentmanga->title;

    updateInfos();
}

void MangaInfoWidget::updateCover()
{
    if (!QFile::exists(currentmanga->coverPath))
    {
        ui->labelMangaInfoCover->clear();
    }
    else
    {
        QPixmap img;
        img.load(currentmanga->coverPath);
        ui->labelMangaInfoCover->setPixmap(
            img.scaled(coversize, coversize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void MangaInfoWidget::updateInfos()
{
    QStringListModel *model = new QStringListModel(this);
    model->setStringList(currentmanga->chapters.getMangaTitlesReversed());

    if (ui->listViewChapters->model() != nullptr)
        ui->listViewChapters->model()->deleteLater();

    ui->listViewChapters->setModel(model);

    ui->labelMangaInfoTitle->setText(currentmanga->title);

    updateLabel(ui->labelMangaInfoLabelAuthor, ui->labelMangaInfoLabelAuthorContent, currentmanga->author);
    updateLabel(ui->labelMangaInfoLabelArtist, ui->labelMangaInfoLabelArtistContent, currentmanga->artist);
    updateLabel(ui->labelMangaInfoLabelGenres, ui->labelMangaInfoLabelGenresContent, currentmanga->genres);
    updateLabel(ui->labelMangaInfoLabelStaus, ui->labelMangaInfoLabelStausContent, currentmanga->status);

    ui->labelMangaInfoLabelSummaryContent->setText(currentmanga->summary);

    ui->scrollAreaMangaInfoSummary->verticalScrollBar()->setValue(0);
    ui->listViewChapters->verticalScrollBar()->setValue(0);

    bool enable = currentmanga->chapters.count() > 0;

    ui->pushButtonReadContinue->setEnabled(enable);
    ui->pushButtonReadFirst->setEnabled(enable);
    ui->pushButtonReadLatest->setEnabled(enable);
}

void MangaInfoWidget::setFavoriteButtonState(bool state)
{
    ui->toolButtonAddFavorites->setChecked(state);
}

void MangaInfoWidget::on_toolButtonAddFavorites_clicked()
{
    if (!currentmanga.isNull())
        emit toggleFavoriteClicked(currentmanga);
}

void MangaInfoWidget::on_listViewChapters_clicked(const QModelIndex &index)
{
    emit readMangaClicked({currentmanga->chapters.count() - 1 - index.row(), 0});
}

void MangaInfoWidget::on_pushButtonReadLatest_clicked()
{
    emit readMangaClicked({currentmanga->chapters.count() - 1, 0});
}

void MangaInfoWidget::on_pushButtonReadContinue_clicked()
{
    emit readMangaContinueClicked();
}

void MangaInfoWidget::on_pushButtonReadFirst_clicked()
{
    emit readMangaClicked({0, 0});
}
