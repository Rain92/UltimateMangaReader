#include "mainwidget.h"

#ifdef KOBO
#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#endif

#include "ui_mainwidget.h"

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::MainWidget),
      core(new UltimateMangaReaderCore(this)),
      lastTab(MangaInfoTab),
      restorefrontlighttimer(),
      virtualKeyboard(new VirtualKeyboard(this)),
      errorMessageWidget(new ErrorMessageWidget(this))
{
    ui->setupUi(this);
    adjustSizes();
    setupVirtualKeyboard();

    ui->mangaReaderWidget->setSettings(&core->settings);

    // DownloadManager
    core->downloadManager->setImageRescaleSize(this->size());

    // Core

    QObject::connect(core, &UltimateMangaReaderCore::error, [this](auto msg) {
        if (!core->settings.hideErrorMessages)
            errorMessageWidget->showError(msg);
    });

    // MangaController
    QObject::connect(core->mangaController,
                     &MangaController::currentMangaChanged, [this](auto info) {
                         ui->mangaInfoWidget->setManga(info);
                         bool state = core->favoritesManager->isFavorite(info);
                         ui->mangaInfoWidget->setFavoriteButtonState(state);
                         setWidgetTab(MangaInfoTab);
                     });

    QObject::connect(
        core->mangaController, &MangaController::completedImagePreloadSignal,
        ui->mangaReaderWidget, &MangaReaderWidget::addImageToCache);

    QObject::connect(
        core->mangaController, &MangaController::currentIndexChanged,
        ui->mangaReaderWidget, &MangaReaderWidget::updateCurrentIndex);

    QObject::connect(core->mangaController,
                     &MangaController::currentImageChanged,
                     ui->mangaReaderWidget, &MangaReaderWidget::showImage);

    QObject::connect(core->mangaController,
                     &MangaController::indexMovedOutOfBounds, this,
                     &MainWidget::readerGoBack);

    QObject::connect(core->mangaController, &MangaController::error,
                     [this](auto msg) {
                         if (!core->settings.hideErrorMessages)
                             errorMessageWidget->showError(msg);
                     });

    // HomeWidget
    ui->homeWidget->setCore(core);

    QObject::connect(ui->homeWidget, &HomeWidget::mangaSourceClicked, core,
                     &UltimateMangaReaderCore::setCurrentMangaSource);

    QObject::connect(ui->homeWidget, &HomeWidget::mangaClicked, core,
                     &UltimateMangaReaderCore::setCurrentManga);

    QObject::connect(core, &UltimateMangaReaderCore::currentMangaSourceChanged,
                     ui->homeWidget, &HomeWidget::currentMangaSourceChanged);

    // MangaInfoWidget
    QObject::connect(
        ui->mangaInfoWidget, &MangaInfoWidget::toggleFavoriteClicked,
        [this](auto info) {
            bool newstate = core->favoritesManager->toggleFavorite(info);
            ui->mangaInfoWidget->setFavoriteButtonState(newstate);
        });

    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::readMangaClicked,
                     core->mangaController, &MangaController::setCurrentIndex);

    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::readMangaClicked,
                     [this]() { setWidgetTab(MangaReaderTab); });

    QObject::connect(ui->mangaInfoWidget,
                     &MangaInfoWidget::readMangaContinueClicked,
                     [this]() { setWidgetTab(MangaReaderTab); });

    // FavoritesWidget
    ui->favoritesWidget->favoritesmanager = core->favoritesManager;

    QObject::connect(ui->favoritesWidget, &FavoritesWidget::favoriteClicked,
                     [this](auto mangainfo, auto jumptoreader) {
                         core->mangaController->setCurrentManga(mangainfo);
                         if (jumptoreader)
                             setWidgetTab(MangaReaderTab);
                     });

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
    QObject::connect(&restorefrontlighttimer, &QTimer::timeout, this,
                     &MainWidget::restoreFrontLight);
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
    ui->pushButtonClose->setFixedHeight(buttonsize);
    ui->pushButtonFavorites->setFixedHeight(buttonsize);
    ui->pushButtonHome->setFixedHeight(buttonsize);
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
    setFrontLight(core->settings.lightValue, core->settings.comflightValue);

    ui->mangaReaderWidget->setFrontLightPanelState(
        core->settings.lightValue, core->settings.comflightValue);
}

void MainWidget::setFrontLight(int light, int comflight)
{
#ifdef KOBO
    KoboPlatformFunctions::setFrontlightLevel(light, comflight);
#endif

    if (core->settings.lightValue != light ||
        core->settings.comflightValue != comflight)
    {
        core->settings.lightValue = light;
        core->settings.comflightValue = comflight;

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

    errorMessageWidget->setMinimumWidth(this->width());
    errorMessageWidget->setMaximumWidth(this->width());

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

void MainWidget::readerGoBack() { setWidgetTab(lastTab); }

void MainWidget::restoreFrontLight()
{
    setFrontLight(core->settings.lightValue, core->settings.comflightValue);
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
