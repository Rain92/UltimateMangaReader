#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <QStringListModel>
#include <QScrollBar>
#include "configs.h"

#include "../koboplatformintegrationplugin/koboplatformfunctions.h"


MainWidget::MainWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainWidget),
    mangasources(),
    currentmanga(),
    favoritesmanager(mangasources),
    settings(),
    lastTab(0),
    closecounter(0),
    closecounterresettimer(),
    restorefrontlighttimer()
{
    ui->setupUi(this);
    setupDirs();


    settings.deserialize();

    downloadmanager = new DownloadManager(this);
    downloadmanager->connect();


//    mangasources.append(new MangaDex(this, downloadmanager));
//    mangasources.append(new MangaPanda(this, downloadmanager));
//    mangasources.append(new MangaTown(this, downloadmanager));
//    mangasources.append(new MangaWindow(this, downloadmanager));
    mangasources.append(new JaiminisBox(this, downloadmanager));



    ui->homeWidget->setMangaSources(&mangasources);
    currentsource = mangasources[0];

    favoritesmanager.deserialize();

    foreach (AbstractMangaSource *ms, mangasources)
        ms->deserializeMangaList();


//    foreach (AbstractMangaSource *ms, mangasources)
//        if (ms->nummangas != ms->mangalist.links.count())
//            qDebug() << ms->name << " nummangas wrong:" << ms->nummangas << ms->mangalist.links.count();

    ui->favoritesWidget->favoritesmanager = &favoritesmanager;


    setupUI();
    setupFrontLight();

//    if (!downloadmanager->connected())
//        downloadmanager->connect();

    QObject::connect(ui->homeWidget, SIGNAL(mangaSourceClicked(AbstractMangaSource *)), this, SLOT(setCurrentSource(AbstractMangaSource *)));
    QObject::connect(ui->homeWidget, SIGNAL(mangaClicked(QString, QString)), this, SLOT(viewMangaInfo(QString, QString)));
    QObject::connect(ui->homeWidget, SIGNAL(favoritesCleared()), &favoritesmanager, SLOT(clearFavorites()));

    QObject::connect(ui->mangaInfoWidget, SIGNAL(toggleFavoriteClicked(QSharedPointer<MangaInfo>)), this, SLOT(toggleFavorite(QSharedPointer<MangaInfo>)));
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

    closecounterresettimer.setSingleShot(true);
    QObject::connect(&closecounterresettimer, SIGNAL(timeout()), this, SLOT(resetCloseCounter()));

    restorefrontlighttimer.setSingleShot(true);
    QObject::connect(&restorefrontlighttimer, SIGNAL(timeout()), this, SLOT(restoreFrontLight()));


}

MainWidget::~MainWidget()
{
    delete ui;
}



void  MainWidget::setupUI()
{
#ifdef KOBO
    //TODO
//    VirtualKeyboard *vk = getVirtualKeyboard();
//    ui->verticalLayoutKeyboardContainer->insertWidget(0, vk);

//    enableVirtualKeyboard(true);
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
    qDebug() << mangalistdir;
    if (!QDir(cachedir).exists())
        QDir().mkpath(cachedir);

    if (!QDir(mangalistdir).exists())
        QDir().mkpath(mangalistdir);
}

void MainWidget::enableVirtualKeyboard(bool enabled)
{
#ifdef KOBO
    //TODO
//    VirtualKeyboard *vk = getVirtualKeyboard();

//    vk->hide();

//    if (enabled)
//        ui->frameKeyboardContainer->show();
//    else
//        ui->frameKeyboardContainer->hide();
#else
    Q_UNUSED(enabled);
#endif
}


void MainWidget::setupFrontLight()
{
    setFrontLight(settings.lightvalue, settings.comflightvalue);

#ifdef KOBO
    //TODO
    ui->mangaReaderWidget->setFrontLightPanelState(0,100,0,0,100,0);
#endif

    ui->mangaReaderWidget->setFrontLightPanelState(settings.lightvalue, settings.comflightvalue);
}

void MainWidget::setFrontLight(int light, int comflight)
{
#ifdef KOBO
        KoboPlatformFunctions::setFrontlightLevel(light, comflight);
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
#ifdef KOBO
    //TODO
//    getVirtualKeyboard()->hide();
#endif

//    qDebug() << "setWidgetTab" << page;

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
            ui->favoritesWidget->showFavoritesList();
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
        if (currentmanga.data() != info.data())
        {
            currentmanga.clear();
            currentmanga = info;
            QObject::connect(currentmanga.data(), SIGNAL(completedImagePreloadSignal(QString)), ui->mangaReaderWidget, SLOT(addImageToCache(QString)));
        }


        ui->mangaInfoWidget->setManga(currentmanga);
        ui->mangaInfoWidget->setFavoriteButtonState(favoritesmanager.isFavorite(currentmanga.data()));

        viewMangaImage(info->currentindex);
    }
    else
    {
        viewMangaInfo(info);
    }
}


void MainWidget::setCurrentSource(AbstractMangaSource *source)
{
    currentsource = source;
}

void MainWidget::viewMangaInfo(QSharedPointer<MangaInfo> info)
{
    if (currentmanga.data() != info.data())
    {
        currentmanga.clear();
        currentmanga = info;
        QObject::connect(currentmanga.data(), SIGNAL(completedImagePreloadSignal(QString)), ui->mangaReaderWidget, SLOT(addImageToCache(QString)));
    }

    ui->mangaInfoWidget->setManga(currentmanga);
    ui->mangaInfoWidget->setFavoriteButtonState(favoritesmanager.isFavorite(currentmanga.data()));

    setWidgetTab(1);

//#ifndef QT_DEBUG
    currentmanga->preloadPopular();
//#endif
}


void MainWidget::viewMangaInfo(const QString &mangalink, const QString &mangatitle)
{
    viewMangaInfo(QSharedPointer<MangaInfo>(currentsource->loadMangaInfo(mangalink, mangatitle)));
}

void MainWidget::toggleFavorite(QSharedPointer<MangaInfo> manga)
{
    ui->mangaInfoWidget->setFavoriteButtonState(favoritesmanager.toggleFavorite(manga));
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
    if (currentmanga->numchapters == 0)
        return;

    if (index.illegal)
        return readerGoBack();

    ui->mangaReaderWidget->showImage(currentmanga->goChapterPage(index));
    ui->mangaReaderWidget->updateReaderLabels(currentmanga);

    setWidgetTab(3);

    currentmanga->preloadNeighbours();
    currentmanga->serializeProgress();
}


void MainWidget::restoreFrontLight()
{
    setFrontLight(settings.lightvalue, settings.comflightvalue);
}

void MainWidget::resetCloseCounter()
{
    closecounter = 0;
}


























