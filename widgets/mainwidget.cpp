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
    currentmanga(nullptr),
    favoritesmanager(),
    lastTab(0)
{
    ui->setupUi(this);
    setupDirs();

    favoritesmanager.deserialize();

    downloadmanager = new DownloadManager(this);


    mangasources.append(new MangaPanda(this, downloadmanager));
    mangasources.append(new MangaDex(this, downloadmanager));
    mangasources.append(new MangaTown(this, downloadmanager));
    mangasources.append(new MangaWindow(this, downloadmanager));



    ui->homeWidget->setMangaSources(&mangasources);
    ui->favoritesWidget->mangasources = mangasources;
    currentsource = mangasources[0];

    downloadmanager->connect();

    foreach (AbstractMangaSource *ms, mangasources)
        ms->deserializeMangaList();


    setupUI();
    setupFrontLight();

    QObject::connect(ui->homeWidget, SIGNAL(mangaSourceClicked(AbstractMangaSource *)), this, SLOT(setCurrentSource(AbstractMangaSource *)));
    QObject::connect(ui->homeWidget, SIGNAL(mangaClicked(QString, QString)), this, SLOT(viewMangaInfo(QString, QString)));

    QObject::connect(ui->mangaInfoWidget, SIGNAL(toggleFavoriteClicked(MangaInfo *)), this, SLOT(toggleFavorite(MangaInfo *)));
    QObject::connect(ui->mangaInfoWidget, SIGNAL(readMangaClicked(MangaIndex)), this, SLOT(viewMangaImage(MangaIndex)));

    QObject::connect(ui->favoritesWidget, SIGNAL(favoriteClicked(Favorite, bool)), this, SLOT(viewFavorite(Favorite, bool)));
    QObject::connect(ui->favoritesWidget, SIGNAL(mangaListUpdated()), &favoritesmanager, SLOT(serialize()));

    QObject::connect(ui->mangaReaderWidget, SIGNAL(changeView(int)), this, SLOT(setWidgetTab(int)));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(advancPageClicked(bool)), this, SLOT(advanceMangaPage(bool)));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(closeApp()), this, SLOT(on_pushButtonClose_clicked()));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(back()), this, SLOT(readerGoBack()));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(frontlightchanged(int, int)), this, SLOT(setFrontLight(int, int)));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(gotoIndex(MangaIndex)), this, SLOT(viewMangaImage(MangaIndex)));
}

MainWidget::~MainWidget()
{
    delete ui;
}

void  MainWidget::setupUI()
{
#ifndef WINDOWS
    initTopLevelWidget(this);

    //ui->pushButtonClose->setText( static_cast<KoboPlatform *>(Platform::get())->a1FlTable.empty()?"true":"false");

    VirtualKeyboard *vk = getVirtualKeyboard();

    ui->verticalLayout->insertWidget(1, vk);

#endif

    downloadmanager->setImageSize(this->width(), this->height());

    adjustSizes();
}

void  MainWidget::adjustSizes()
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


void MainWidget::setupFrontLight()
{
#ifndef WINDOWS
    ui->mangaReaderWidget->setFrontLightPanelState(
                Platform::get()->frontlightGetMinLevel(),
                Platform::get()->frontlightGetMaxLevel(),
                Platform::get()->frontlightGetLevel(),
                Platform::get()->frontlightGetMinTemp(),
                Platform::get()->frontlightGetMaxTemp(),
                Platform::get()->frontlightGetTemp());
#endif
}

void MainWidget::setFrontLight(int light, int comflight)
{
#ifndef WINDOWS
    if (light > Platform::get()->frontlightGetMinLevel())
    {
        if(!Platform::get()->frontlightIsOn())
            Platform::get()->frontlightSetOn(true);

        Platform::get()->frontlightSetLevel(light, comflight);

    }
    else
    {
        Platform::get()->frontlightSetOn(false);
    }
#else
    Q_UNUSED(light)
    Q_UNUSED(comflight)
#endif
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

    if (currentmanga != nullptr)
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



void MainWidget::viewFavorite(Favorite fav, bool current)
{

    foreach (AbstractMangaSource *source, mangasources)
        if (fav.hostname == source->name)
            currentsource = source;

    if (current)
    {
        if (currentmanga != nullptr)
            delete currentmanga;
        currentmanga = currentsource->loadMangaInfo(fav.mangalink, fav.title);
        QObject::connect(currentmanga, SIGNAL(completedImagePreloadSignal(QString)), ui->mangaReaderWidget, SLOT(addImageToCache(QString)));


        ui->mangaInfoWidget->setManga(currentmanga);
        ui->mangaInfoWidget->setFavoriteButtonState(!favoritesmanager.isFavorite(currentmanga));

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

void MainWidget::viewMangaInfo(const QString &mangalink, const QString &mangatitle)
{
//    if (currentmanga != nullptr)
//        delete currentmanga;
    currentmanga = currentsource->loadMangaInfo(mangalink, mangatitle);
    QObject::connect(currentmanga, SIGNAL(completedImagePreloadSignal(QString)), ui->mangaReaderWidget, SLOT(addImageToCache(QString)));

    ui->mangaInfoWidget->setManga(currentmanga);
    ui->mangaInfoWidget->setFavoriteButtonState(!favoritesmanager.isFavorite(currentmanga));

    setWidgetTab(1);

#ifndef QT_DEBUG
    currentmanga->preloadPopular();
#endif
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






























