#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <QStringListModel>
#include <QScrollBar>
#include "configs.h"





MainWidget::MainWidget(QWidget *parent) :
#ifdef WINDOWS
    QWidget(parent),
#endif
    ui(new Ui::MainWidget),
    currentmanga(),
    favoritesmanager(),
    settings(),
    lastTab(0)
{
    ui->setupUi(this);
    setupDirs();

    favoritesmanager.deserialize();
    settings.deserialize();

    downloadmanager = new DownloadManager(this);


    mangasources.append(new MangaPanda(this, downloadmanager));
    mangasources.append(new MangaTown(this, downloadmanager));
    mangasources.append(new MangaWindow(this, downloadmanager));
    mangasources.append(new MangaDex(this, downloadmanager));



    ui->homeWidget->setMangaSources(&mangasources);
    ui->favoritesWidget->mangasources = mangasources;
    currentsource = mangasources[0];


    foreach (AbstractMangaSource *ms, mangasources)
        ms->deserializeMangaList();


    setupUI();
    setupFrontLight();

    if (!downloadmanager->connected())
        downloadmanager->connect();

    QObject::connect(ui->homeWidget, SIGNAL(mangaSourceClicked(AbstractMangaSource *)), this, SLOT(setCurrentSource(AbstractMangaSource *)));
    QObject::connect(ui->homeWidget, SIGNAL(mangaClicked(QString, QString)), this, SLOT(viewMangaInfo(QString, QString)));

    QObject::connect(ui->mangaInfoWidget, SIGNAL(toggleFavoriteClicked(MangaInfo *)), this, SLOT(toggleFavorite(MangaInfo *)));
    QObject::connect(ui->mangaInfoWidget, SIGNAL(readMangaClicked(MangaIndex)), this, SLOT(viewMangaImage(MangaIndex)));

    QObject::connect(ui->favoritesWidget, SIGNAL(favoriteClicked(QSharedPointer<MangaInfo>, bool)), this, SLOT(viewFavorite(QSharedPointer<MangaInfo>, bool)));
    QObject::connect(ui->favoritesWidget, SIGNAL(mangaListUpdated()), &favoritesmanager, SLOT(serialize()));

    QObject::connect(ui->mangaReaderWidget, SIGNAL(changeView(int)), this, SLOT(setWidgetTab(int)));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(advancPageClicked(bool)), this, SLOT(advanceMangaPage(bool)));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(closeApp()), this, SLOT(on_pushButtonClose_clicked()));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(back()), this, SLOT(readerGoBack()));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(frontlightchanged(int, int)), this, SLOT(setFrontLight(int, int)));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(gotoIndex(MangaIndex)), this, SLOT(viewMangaImage(MangaIndex)));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(enableVirtualKeyboard(bool)), this, SLOT(enableVirtualKeyboard(bool)));

}

MainWidget::~MainWidget()
{
    delete ui;
}



void  MainWidget::setupUI()
{
#ifndef WINDOWS
    initTopLevelWidget(this);

    enableVirtualKeyboard(true);
#endif

    downloadmanager->setImageSize(this->width(), this->height());

    adjustSizes();
}

void MainWidget::adjustSizes()
{
    ui->pushButtonClose->setMinimumHeight(buttonsize);
    ui->pushButtonFavorites->setMinimumHeight(buttonsize);
    ui->pushButtonHome->setMinimumHeight(buttonsize);
}

void MainWidget::setupDirs()
{
    if (!QDir(cachedir).exists())
        QDir().mkpath(cachedir);

    if (!QDir(mangalistdir).exists())
        QDir().mkpath(mangalistdir);
}

void MainWidget::enableVirtualKeyboard(bool enabled)
{
#ifndef WINDOWS
    VirtualKeyboard *vk = getVirtualKeyboard();

    if (enabled)
        ui->verticalLayout->insertWidget(1, vk);
    else
        ui->verticalLayout->removeWidget(vk);

    vk-> hide();
#else
    Q_UNUSED(enabled);
#endif
}


void MainWidget::setupFrontLight()
{
    setFrontLight(settings.lightvalue, settings.comflightvalue);

#ifndef WINDOWS
    ui->mangaReaderWidget->setFrontLightPanelState(
        Platform::get()->frontlightGetMinLevel(),
        Platform::get()->frontlightGetMaxLevel(),
        Platform::get()->frontlightGetLevel(),
        Platform::get()->frontlightGetMinTemp(),
        Platform::get()->frontlightGetMaxTemp(),
        Platform::get()->frontlightGetTemp());
#endif

    ui->mangaReaderWidget->setFrontLightPanelState(settings.lightvalue, settings.comflightvalue);
}

void MainWidget::setFrontLight(int light, int comflight)
{
//    qDebug() << light << comflight;
#ifndef WINDOWS
    if (light > Platform::get()->frontlightGetMinLevel())
    {
        Platform::get()->frontlightSetLevel(light, comflight);

        if (!Platform::get()->frontlightIsOn())
            Platform::get()->frontlightSetOn(true);
    }
    else
    {
        Platform::get()->frontlightSetOn(false);
    }
#else
    Q_UNUSED(light)
    Q_UNUSED(comflight)
#endif
    settings.lightvalue = light;
    settings.comflightvalue = comflight;

    settings.scheduleSerialize();
}



void MainWidget::on_pushButtonHome_clicked()
{
    setWidgetTab(0);
}

void MainWidget::on_pushButtonFavorites_clicked()
{
    setWidgetTab(2);
}

void MainWidget::on_pushButtonClose_clicked()
{
    close();
}

void MainWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    downloadmanager->setImageSize(this->width(), this->height());
}



void MainWidget::setWidgetTab(int page)
{
#ifndef WINDOWS
    getVirtualKeyboard()->hide();
#endif

    if (page == ui->stackedWidget->currentIndex())
        return;

    if (!currentmanga.isNull())
        currentmanga->cancelAllPreloads();

    if (page == 3)
    {
        ui->navigationBar->setVisible(false);
        ui->stackedWidget->setCurrentIndex(page);
    }
    else
    {
        if (page == 1)
        {
            lastTab = 1;
        }
        if (page == 2)
        {
            ui->favoritesWidget->showFavoritesList(favoritesmanager.getFavorites());
            lastTab = 2;
        }

        ui->navigationBar->setVisible(true);
        ui->stackedWidget->setCurrentIndex(page);
    }

}

void MainWidget::viewFavorite(QSharedPointer<MangaInfo> info, bool current)
{
    foreach (AbstractMangaSource *source, mangasources)
        if (info->hostname == source->name)
            currentsource = source;

    if (current)
    {
        currentmanga.clear();
        currentmanga = info;
        QObject::connect(currentmanga.data(), SIGNAL(completedImagePreloadSignal(QString)), ui->mangaReaderWidget, SLOT(addImageToCache(QString)));


        ui->mangaInfoWidget->setManga(currentmanga);
        ui->mangaInfoWidget->setFavoriteButtonState(!favoritesmanager.isFavorite(currentmanga.data()));

        viewMangaImage(info->currentindex);
    }
    else
    {
        viewMangaInfo(info);
    }
}

void MainWidget::viewFavorite(Favorite fav, bool current)
{

    foreach (AbstractMangaSource *source, mangasources)
        if (fav.hostname == source->name)
            currentsource = source;

    if (current)
    {
        currentmanga.clear();
        currentmanga = QSharedPointer<MangaInfo>(currentsource->loadMangaInfo(fav.mangalink, fav.title));
        QObject::connect(currentmanga.data(), SIGNAL(completedImagePreloadSignal(QString)), ui->mangaReaderWidget, SLOT(addImageToCache(QString)));


        ui->mangaInfoWidget->setManga(currentmanga);
        ui->mangaInfoWidget->setFavoriteButtonState(!favoritesmanager.isFavorite(currentmanga.data()));

        viewMangaImage(fav.currentindex);
    }
    else
    {
        viewMangaInfo(fav.mangalink, fav.title);
    }
}

void MainWidget::setCurrentSource(AbstractMangaSource *source)
{
    currentsource = source;
}

void MainWidget::viewMangaInfo(QSharedPointer<MangaInfo> info)
{
    currentmanga.clear();
    currentmanga = info;
    QObject::connect(currentmanga.data(), SIGNAL(completedImagePreloadSignal(QString)), ui->mangaReaderWidget, SLOT(addImageToCache(QString)));

    ui->mangaInfoWidget->setManga(currentmanga);
    ui->mangaInfoWidget->setFavoriteButtonState(!favoritesmanager.isFavorite(currentmanga.data()));

    setWidgetTab(1);

//#ifndef QT_DEBUG
    currentmanga->preloadPopular();
//#endif
}


void MainWidget::viewMangaInfo(const QString &mangalink, const QString &mangatitle)
{
    viewMangaInfo(QSharedPointer<MangaInfo>(currentsource->loadMangaInfo(mangalink, mangatitle)));
}

void MainWidget::toggleFavorite(MangaInfo *manga)
{
    ui->mangaInfoWidget->setFavoriteButtonState(!favoritesmanager.toggleFavorite(manga));
}


void MainWidget::advanceMangaPage(bool direction)
{
    if (direction)
        viewMangaImage(currentmanga->currentindex.nextPageIndex(&currentmanga->chapters));
    else
        viewMangaImage(currentmanga->currentindex.prevPageIndex(&currentmanga->chapters));

}

void MainWidget::readerGoBack()
{
    setWidgetTab(lastTab);
}


void MainWidget::viewMangaImage(const MangaIndex &index)
{
    if (index.illegal)
        return readerGoBack();

    ui->mangaReaderWidget->showImage(currentmanga->goChapterPage(index));
    ui->mangaReaderWidget->updateReaderLabels(currentmanga);

    setWidgetTab(3);

    currentmanga->preloadNeighbours();
    currentmanga->serializeProgress();
}






























