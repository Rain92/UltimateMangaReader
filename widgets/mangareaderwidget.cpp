#include "mangareaderwidget.h"
#include "ui_mangareaderwidget.h"
#include "configs.h"

MangaReaderWidget::MangaReaderWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MangaReaderWidget),
    imgcache(),
    imgcachepaths()
{
    ui->setupUi(this);
    adjustSizes();

    ui->readerNavigationBar->setVisible(false);
}

MangaReaderWidget::~MangaReaderWidget()
{
    delete ui;
}


void  MangaReaderWidget::adjustSizes()
{
    ui->pushButtonReaderBack->setMinimumHeight(buttonsize);
    ui->pushButtonReaderClose->setMinimumHeight(buttonsize);
    ui->pushButtonReaderHome->setMinimumHeight(buttonsize);
}

void MangaReaderWidget::updateReaderLabels(MangaInfo *currentmanga)
{
    ui->labelReaderChapter->setText("Chapter: " + QString::number(currentmanga->currentindex.chapter + 1) +
                                    "/" + QString::number(currentmanga->numchapters));
    ui->labelReaderPage->setText("Page: " + QString::number(currentmanga->currentindex.page + 1) + "/" +
                                 QString::number(currentmanga->chapters.at(currentmanga->currentindex.chapter).numpages));
}


void MangaReaderWidget::on_pushButtonReaderHome_clicked()
{
    emit changeView(0);
}

void MangaReaderWidget::on_pushButtonReaderBack_clicked()
{
    emit back();
}

void MangaReaderWidget::on_pushButtonReaderClose_clicked()
{
    emit closeApp();
}

void MangaReaderWidget::showImage(const QString &path)
{
    ui->readerNavigationBar->setVisible(false);

    if (path != "")
    {
        int i = imgcachepaths.indexOf(path);

        if (i != -1)
        {
            qDebug() << "cachehit!" << i;
            ui->mangaImageContainer->setImage(*imgcache[i]);
        }
        else
        {
            addImageToCache(path);
            ui->mangaImageContainer->setImage(*imgcache[0]);
        }
    }
    else
    {
        emit changeView(1);
    }
}

void MangaReaderWidget::addImageToCache(const QString &path)
{
    int i = imgcachepaths.indexOf(path);
    if (i != -1)
    {
        imgcachepaths.move(i, 0);
        imgcache.move(i, 0);
    }
    else
    {
        imgcachepaths.insert(0, path);
        imgcache.insert(0, new QPixmap(path));

        if (imgcache.count() > imagecachesize)
        {
            imgcachepaths.removeLast();
            delete imgcache.last();
            imgcache.removeLast();
        }
    }
}

void MangaReaderWidget::on_mangaImageContainer_clicked(QPoint pos)
{
    if (ui->readerNavigationBar->isVisible())
    {
        ui->readerNavigationBar->setVisible(false);
        return;
    }

    if (pos.y() > this->height() * readerbottommenuethreshold)
    {
        ui->readerNavigationBar->setVisible(true);
    }
    else
    {
        if (pos.x() > this->width() * readerpreviouspagethreshold)
        {
            emit advancPageClicked(true);
        }
        else
        {
            emit advancPageClicked(false);
        }
    }
}
