#include "mainwidget.h"

#include <QScrollBar>
#include <QStringListModel>

#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#include "ui_mainwidget.h"

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::MainWidget),
      core(new UltimateMangaReaderCore(this)),
      lastTab(MangaInfoTab),
      restorefrontlighttimer(),
      virtualKeyboard(new VirtualKeyboard(this))
{
    ui->setupUi(this);
    adjustSizes();
    setupVirtualKeyboard();

    // DownloadManager
    core->downloadManager->setImageRescaleSize(this->size());

    // MangaController
    QObject::connect(core->mangaController,
                     &MangaController::currentMangaChanged, ui->mangaInfoWidget,
                     &MangaInfoWidget::setManga);

    QObject::connect(core->mangaController,
                     &MangaController::currentMangaChanged,
                     [this]() { setWidgetTab(MangaInfoTab); });

    QObject::connect(
        core->mangaController, &MangaController::completedImagePreloadSignal,
        ui->mangaReaderWidget, &MangaReaderWidget::addImageToCache);

    QObject::connect(
        core->mangaController, &MangaController::currentIndexChanged,
        ui->mangaReaderWidget, &MangaReaderWidget::updateReaderLabels);

    QObject::connect(core->mangaController,
                     &MangaController::currentImageChanged,
                     ui->mangaReaderWidget, &MangaReaderWidget::showImage);

    QObject::connect(core->mangaController,
                     &MangaController::indexMovedOutOfBounds, this,
                     &MainWidget::readerGoBack);

    // HomeWidget
    ui->homeWidget->setMangaSources(core->activeMangaSources.values());

    QObject::connect(ui->homeWidget, &HomeWidget::mangaSourceClicked, core,
                     &UltimateMangaReaderCore::setCurrentMangaSource);

    QObject::connect(ui->homeWidget, &HomeWidget::mangaClicked, core,
                     &UltimateMangaReaderCore::setCurrentManga);
    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::readMangaClicked,
                     [this]() { setWidgetTab(MangaInfoTab); });

    QObject::connect(ui->homeWidget, SIGNAL(favoritesCleared()),
                     core->favoritesManager, SLOT(clearFavorites()));

    // MangaInfoWidget
    //    QObject::connect(ui->mangaInfoWidget,
    //                     SIGNAL(toggleFavoriteClicked(QSharedPointer<MangaInfo>)),
    //                     this,
    //                     SLOT(toggleFavorite(QSharedPointer<MangaInfo>)));

    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::readMangaClicked,
                     core->mangaController, &MangaController::setCurrentIndex);
    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::readMangaClicked,
                     [this]() { setWidgetTab(MangaReaderTab); });

    QObject::connect(ui->mangaInfoWidget,
                     &MangaInfoWidget::readMangaContinueClicked,
                     [this]() { setWidgetTab(MangaReaderTab); });

    // FavoritesWidget
    ui->favoritesWidget->favoritesmanager = core->favoritesManager;

    // TODO
    //    QObject::connect(ui->favoritesWidget,
    //                     SIGNAL(favoriteClicked(QSharedPointer<MangaInfo>,
    //                     bool)), this,
    //                     SLOT(viewFavorite(QSharedPointer<MangaInfo>, bool)));
    //    QObject::connect(ui->favoritesWidget, SIGNAL(mangaListUpdated()),
    //                     core->favoritesManager, SLOT(serialize()));

    // MangaReaderWidget
    QObject::connect(ui->mangaReaderWidget, &MangaReaderWidget::changeView,
                     this, &MainWidget::setWidgetTab);

    QObject::connect(ui->mangaReaderWidget,
                     &MangaReaderWidget::advancPageClicked,
                     core->mangaController, &MangaController::advanceMangaPage);
    QObject::connect(ui->mangaReaderWidget, &MangaReaderWidget::closeApp, this,
                     &MainWidget::on_pushButtonClose_clicked);
    QObject::connect(ui->mangaReaderWidget, &MangaReaderWidget::back, this,
                     &MainWidget::readerGoBack);
    QObject::connect(ui->mangaReaderWidget,
                     &MangaReaderWidget::frontlightchanged, this,
                     &MainWidget::setFrontLight);
    QObject::connect(ui->mangaReaderWidget, &MangaReaderWidget::gotoIndex,
                     core->mangaController, &MangaController::setCurrentIndex);

    // FrontLight
    setupFrontLight();
    restorefrontlighttimer.setSingleShot(true);
    QObject::connect(&restorefrontlighttimer, SIGNAL(timeout()), this,
                     SLOT(restoreFrontLight()));
}

MainWidget::~MainWidget() { delete ui; }

void MainWidget::setupVirtualKeyboard()
{
    virtualKeyboard->hide();
    ui->verticalLayoutKeyboardContainer->insertWidget(0, virtualKeyboard);

    ui->homeWidget->installEventFilter(this);
}

void MainWidget::adjustSizes()
{
    ui->pushButtonClose->setMinimumHeight(buttonsize);
    ui->pushButtonFavorites->setMinimumHeight(buttonsize);
    ui->pushButtonHome->setMinimumHeight(buttonsize);
}

void MainWidget::enableVirtualKeyboard(bool enabled)
{
#ifdef KOBO
    if (enabled)
        virtualKeyboard->show();
    else
        virtualKeyboard->hide();
#else
    Q_UNUSED(enabled)
#endif
}

void MainWidget::setupFrontLight()
{
    setFrontLight(core->settings.lightvalue, core->settings.comflightvalue);

    ui->mangaReaderWidget->setFrontLightPanelState(
        core->settings.lightvalue, core->settings.comflightvalue);
}

void MainWidget::setFrontLight(int light, int comflight)
{
#ifdef KOBO
    KoboPlatformFunctions::setFrontlightLevel(light, comflight);
#endif

    if (core->settings.lightvalue != light ||
        core->settings.comflightvalue != comflight)
    {
        core->settings.lightvalue = light;
        core->settings.comflightvalue = comflight;

        core->settings.scheduleSerialize();
    }
}

void MainWidget::on_pushButtonHome_clicked() { setWidgetTab(HomeTab); }

void MainWidget::on_pushButtonFavorites_clicked()
{
    setWidgetTab(FavoritesTab);
}

void MainWidget::on_pushButtonClose_clicked() { close(); }

void MainWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    core->downloadManager->setImageRescaleSize(this->size());
}

void MainWidget::setWidgetTab(WidgetTab tab)
{
    enableVirtualKeyboard(false);

    if (tab == ui->stackedWidget->currentIndex())
        return;

    if (tab == MangaReaderTab)
    {
        ui->navigationBar->setVisible(false);
        ui->stackedWidget->setCurrentIndex(tab);
    }
    else
    {
        if (tab == MangaInfoTab)
        {
            lastTab = MangaInfoTab;
        }
        else if (tab == FavoritesTab)
        {
            ui->favoritesWidget->showFavoritesList();
            lastTab = FavoritesTab;
        }

        ui->navigationBar->setVisible(true);
        ui->stackedWidget->setCurrentIndex(tab);
    }
}

void MainWidget::viewFavorite(QSharedPointer<MangaInfo> info, bool current)
{
    // TODO
    //    foreach (AbstractMangaSource *source, mangasources)
    //        if (info->hostname == source->name)
    //            currentsource = source;

    //    if (current)
    //    {
    //        if (currentmanga.get() != info.get())
    //        {
    //            currentmanga.clear();
    //            currentmanga = info;
    //            QObject::connect(currentmanga.get(),
    //                             SIGNAL(completedImagePreloadSignal(QString)),
    //                             ui->mangaReaderWidget,
    //                             SLOT(addImageToCache(QString)));
    //        }

    //        ui->mangaInfoWidget->setManga(currentmanga);
    //        ui->mangaInfoWidget->setFavoriteButtonState(
    //            favoritesmanager.isFavorite(currentmanga.get()));

    //        viewMangaImage(info->currentIndex);
    //    }
    //    else
    //    {
    //        viewMangaInfo(info);
    //    }
}

void MainWidget::readerGoBack() { setWidgetTab(lastTab); }

void MainWidget::restoreFrontLight()
{
    setFrontLight(core->settings.lightvalue, core->settings.comflightvalue);
}

bool MainWidget::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() == QEvent::RequestSoftwareInputPanel + 1000)
    {
        enableVirtualKeyboard(true);
        return true;
    }
    else if (ev->type() == QEvent::CloseSoftwareInputPanel + 1000)
    {
        enableVirtualKeyboard(false);
        return true;
    }
    return false;

    Q_UNUSED(obj)
}
