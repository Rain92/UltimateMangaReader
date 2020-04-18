#include "mainwidget.h"

#include "ui_mainwidget.h"

#ifdef KOBO
#include "../koboplatformintegrationplugin/kobokey.h"
#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#define POWERBUTTON KoboKey::Key_Power
#define SLEEPCOVERBUTTON KoboKey::Key_SleepCover
#else
#define POWERBUTTON Qt::Key_F1
#define SLEEPCOVERBUTTON Qt::Key_F2
#endif

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::MainWidget),
      core(new UltimateMangaReaderCore(this)),
      lastTab(MangaInfoTab),
      virtualKeyboard(new VirtualKeyboard(this)),
      errorMessageWidget(new ErrorMessageWidget(this)),
      powerButtonTimer(new QTimer(this)),
      suspendManager(new SuspendManager(this))
{
    ui->setupUi(this);
    adjustSizes();
    ui->batteryIcon->updateIcon();
    setupVirtualKeyboard();

    QObject::connect(powerButtonTimer, &QTimer::timeout, this, &MainWidget::close);

    // Dialogs
    menuDialog = new MenuDialog(this);
    settingsDialog = new SettingsDialog(this);
    updateMangaListsDialog = new UpdateMangaListsDialog(this);
    clearCacheDialog = new ClearCacheDialog(this);
    wifiDialog = new WifiDialog(this, core->downloadManager);
    screensaverDialog = new ScreensaverDialog(this);
    downloadMangaChaptersDialog = new DownloadMangaChaptersDialog(this);

    updateMangaListsDialog->setSettings(&core->settings);

    QObject::connect(menuDialog, &MenuDialog::finished,
                     [this](int b) { menuDialogButtonPressed(static_cast<MenuButton>(b)); });

    QObject::connect(clearCacheDialog, &MenuDialog::finished,
                     [this](int l) { core->clearDownloadCache(static_cast<ClearDownloadCacheLevel>(l)); });

    QObject::connect(updateMangaListsDialog, &UpdateMangaListsDialog::updateClicked, core,
                     &UltimateMangaReaderCore::updateMangaLists);

    // DownloadManager
    core->downloadManager->setImageRescaleSize(this->size());
    QObject::connect(core->downloadManager, &DownloadManager::connectionStatusChanged,
                     [this](bool connected) {
                         auto pic = connected ? ":/images/icons/wifi.png" : ":/images/icons/no-wifi.png";
                         ui->labelWifiIcon->setPixmap(QPixmap(pic));
                     });

    // Core
    QObject::connect(core, &UltimateMangaReaderCore::error, [this](auto msg) {
        if (!core->settings.hideErrorMessages)
            errorMessageWidget->showError(msg);
    });

    QObject::connect(core, &UltimateMangaReaderCore::timeTick, [this]() {
        ui->batteryIcon->updateIcon();
        ui->mangaReaderWidget->updateMenuBar();
    });

    QObject::connect(core, &UltimateMangaReaderCore::activeMangaSourcesChanged, ui->homeWidget,
                     &HomeWidget::updateSourcesList);

    // MangaController
    QObject::connect(core->mangaController, &MangaController::currentMangaChanged, [this](auto info) {
        ui->mangaInfoWidget->setManga(info);
        bool state = core->favoritesManager->isFavorite(info);
        ui->mangaInfoWidget->setFavoriteButtonState(state);
        setWidgetTab(MangaInfoTab);
    });

    QObject::connect(core->mangaController, &MangaController::completedImagePreloadSignal,
                     ui->mangaReaderWidget, &MangaReaderWidget::addImageToCache);

    QObject::connect(core->mangaController, &MangaController::currentIndexChanged, ui->mangaReaderWidget,
                     &MangaReaderWidget::updateCurrentIndex);

    QObject::connect(core->mangaController, &MangaController::currentImageChanged, ui->mangaReaderWidget,
                     &MangaReaderWidget::showImage);

    QObject::connect(core->mangaController, &MangaController::indexMovedOutOfBounds, this,
                     &MainWidget::readerGoBack);

    QObject::connect(core->mangaController, &MangaController::error, [this](auto msg) {
        if (!core->settings.hideErrorMessages)
            errorMessageWidget->showError(msg);
    });

    // HomeWidget
    QObject::connect(ui->homeWidget, &HomeWidget::mangaSourceClicked, core,
                     &UltimateMangaReaderCore::setCurrentMangaSource);

    QObject::connect(ui->homeWidget, &HomeWidget::mangaClicked, core,
                     &UltimateMangaReaderCore::setCurrentManga);

    QObject::connect(core, &UltimateMangaReaderCore::currentMangaSourceChanged, ui->homeWidget,
                     &HomeWidget::currentMangaSourceChanged);

    // MangaInfoWidget
    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::toggleFavoriteClicked, [this](auto info) {
        bool newstate = core->favoritesManager->toggleFavorite(info);
        ui->mangaInfoWidget->setFavoriteButtonState(newstate);
    });

    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::readMangaClicked, [this](auto index) {
        setWidgetTab(MangaReaderTab);
        core->mangaController->setCurrentIndex(index);
    });

    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::readMangaContinueClicked,
                     [this]() { setWidgetTab(MangaReaderTab); });

    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::downloadMangaClicked, downloadMangaChaptersDialog,
                     &DownloadMangaChaptersDialog::show);

    // FavoritesWidget
    ui->favoritesWidget->favoritesmanager = core->favoritesManager;

    QObject::connect(ui->favoritesWidget, &FavoritesWidget::favoriteClicked,
                     [this](auto mangainfo, auto jumptoreader) {
                         core->mangaController->setCurrentManga(mangainfo);
                         if (jumptoreader)
                             setWidgetTab(MangaReaderTab);
                     });

    // MangaReaderWidget
    ui->mangaReaderWidget->setSettings(&core->settings);

    QObject::connect(ui->mangaReaderWidget, &MangaReaderWidget::changeView, this, &MainWidget::setWidgetTab);

    QObject::connect(ui->mangaReaderWidget, &MangaReaderWidget::advancPageClicked, core->mangaController,
                     &MangaController::advanceMangaPage);

    QObject::connect(ui->mangaReaderWidget, &MangaReaderWidget::closeApp, this,
                     &MainWidget::on_pushButtonClose_clicked);

    QObject::connect(ui->mangaReaderWidget, &MangaReaderWidget::back, this, &MainWidget::readerGoBack);

    QObject::connect(ui->mangaReaderWidget, &MangaReaderWidget::frontlightchanged, this,
                     &MainWidget::setFrontLight);

    QObject::connect(ui->mangaReaderWidget, &MangaReaderWidget::gotoIndex, core->mangaController,
                     &MangaController::setCurrentIndex);

    // SettingsDialog
    settingsDialog->setSettings(&core->settings);
    QObject::connect(settingsDialog, &SettingsDialog::activeMangasChanged, core,
                     &UltimateMangaReaderCore::updateActiveScources);

    // SuspendManager

    QObject::connect(suspendManager, &SuspendManager::suspending, this, &MainWidget::onSuspend);
    QObject::connect(suspendManager, &SuspendManager::resuming, this, &MainWidget::onResume);
}

MainWidget::~MainWidget()
{
    delete ui;
}

void MainWidget::adjustSizes()
{
    ui->pushButtonClose->setFixedHeight(buttonsize);
    ui->pushButtonFavorites->setFixedHeight(buttonsize);
    ui->pushButtonHome->setFixedHeight(buttonsize);
    ui->pushButtonHome->setIconSize(QSize(mm_to_px(4), mm_to_px(4)));
    ui->pushButtonFavorites->setIconSize(QSize(mm_to_px(4), mm_to_px(4)));

    ui->toolButtonMenu->setFixedSize(QSize(menuiconsize, menuiconsize));
    ui->toolButtonMenu->setIconSize(QSize(menuiconsize, menuiconsize));
    ui->labelWifiIcon->setFixedSize(QSize(resourceiconsize, resourceiconsize));

    ui->labelSpacer->setFixedSize(ui->batteryIcon->size());

    ui->labelTitle->setStyleSheet("font-size: 18pt");
}

void MainWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    core->updateActiveScources();

    QTimer::singleShot(100, this, &MainWidget::onResume);
}

bool MainWidget::event(QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
        return buttonPressEvent(static_cast<QKeyEvent *>(event));
    else if (event->type() == QEvent::KeyRelease)
        return buttonReleaseEvent(static_cast<QKeyEvent *>(event));
    return QWidget::event(event);
}

bool MainWidget::buttonReleaseEvent(QKeyEvent *event)
{
    QTime::currentTime().msec();

    if (event->key() == POWERBUTTON)
    {
        qDebug() << "Powerkey release";
        powerButtonTimer->stop();

        if (!suspendManager->sleeping)
            suspendManager->suspend();
        else
            suspendManager->resume();
    }
    else if (event->key() == SLEEPCOVERBUTTON)
    {
        qDebug() << "Sleepcover opened";
        //        suspendManager->resume();

        return true;
    }

    return false;
}

bool MainWidget::buttonPressEvent(QKeyEvent *event)
{
    if (event->key() == POWERBUTTON)
    {
        qDebug() << "Powerkey press";
        powerButtonTimer->start(2000);

        return true;
    }
    else if (event->key() == SLEEPCOVERBUTTON)
    {
        qDebug() << "Sleepcover closed";
        suspendManager->suspend();

        return true;
    }

    return false;
}

void MainWidget::onSuspend()
{
    core->enableTimer(false);

    screensaverDialog->showRandomScreensaver();

    disableFrontLight();

    core->downloadManager->disconnectWifi();
}

void MainWidget::onResume()
{
    screensaverDialog->close();

    core->enableTimer(true);

    wifiDialog->connect();
    QTimer::singleShot(200, [this]() {
        setupFrontLight();
        if (!core->downloadManager->connected)
            wifiDialog->open();
    });
}

void MainWidget::setupVirtualKeyboard()
{
    virtualKeyboard->hide();
    ui->verticalLayoutKeyboardContainer->insertWidget(0, virtualKeyboard);

    ui->homeWidget->installEventFilter(this);
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

void MainWidget::disableFrontLight()
{
#ifdef KOBO
    KoboPlatformFunctions::setFrontlightLevel(0, 0);
#endif
}
void MainWidget::setupFrontLight()
{
    setFrontLight(core->settings.lightValue, core->settings.comflightValue);

    ui->mangaReaderWidget->setFrontLightPanelState(core->settings.lightValue, core->settings.comflightValue);
}

void MainWidget::setFrontLight(int light, int comflight)
{
#ifdef KOBO
    KoboPlatformFunctions::setFrontlightLevel(light, comflight);
#endif

    if (core->settings.lightValue != light || core->settings.comflightValue != comflight)
    {
        core->settings.lightValue = light;
        core->settings.comflightValue = comflight;

        core->settings.scheduleSerialize();
    }
}

void MainWidget::on_pushButtonHome_clicked()
{
    setWidgetTab(HomeTab);
}

void MainWidget::on_pushButtonFavorites_clicked()
{
    setWidgetTab(FavoritesTab);
}

void MainWidget::on_pushButtonClose_clicked()
{
    close();
}

void MainWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    errorMessageWidget->setFixedWidth(this->width());

    core->downloadManager->setImageRescaleSize(this->size());
}

void MainWidget::setWidgetTab(WidgetTab tab)
{
    enableVirtualKeyboard(false);

    if (tab == ui->stackedWidget->currentIndex())
        return;

    switch (tab)
    {
        case HomeTab:
            ui->navigationBar->setVisible(true);
            ui->frameHeader->setVisible(true);
            lastTab = HomeTab;
            break;
        case FavoritesTab:
            ui->favoritesWidget->showFavoritesList();
            ui->navigationBar->setVisible(true);
            ui->frameHeader->setVisible(true);
            lastTab = FavoritesTab;
            break;
        case MangaInfoTab:
            ui->navigationBar->setVisible(true);
            ui->frameHeader->setVisible(false);
            lastTab = MangaInfoTab;
            break;
        case MangaReaderTab:
            ui->navigationBar->setVisible(false);
            ui->frameHeader->setVisible(false);
            break;
    }
    ui->frameHeader->repaint();

    ui->batteryIcon->updateIcon();
    ui->stackedWidget->setCurrentIndex(tab);
}

void MainWidget::readerGoBack()
{
    setWidgetTab(lastTab);
}

bool MainWidget::eventFilter(QObject *, QEvent *ev)
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
}

void MainWidget::on_toolButtonMenu_clicked()
{
    menuDialog->move(this->mapToGlobal({0, 0}));
    menuDialog->open();
}

void MainWidget::menuDialogButtonPressed(MenuButton button)
{
    switch (button)
    {
        case ExitButton:
            close();
            break;
        case SettingsButton:
            settingsDialog->resetUI();
            settingsDialog->open();
            break;
        case ClearDownloadsButton:
            clearCacheDialog->setValues(core->getCacheSize(), core->getFreeSpace());
            clearCacheDialog->open();
            break;
        case UpdateMangaListsButton:
            updateMangaListsDialog->resetUI();
            updateMangaListsDialog->open();
            break;
    }
}
