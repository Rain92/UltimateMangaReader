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
    adjustUI();
    ui->batteryIcon->updateIcon();
    setupVirtualKeyboard();

    QObject::connect(powerButtonTimer, &QTimer::timeout, this, &MainWidget::close);

    // Dialogs
    menuDialog = new MenuDialog(this);
    settingsDialog = new SettingsDialog(&core->settings, this);
    updateMangaListsDialog = new UpdateMangaListsDialog(&core->settings, this);
    clearCacheDialog = new ClearCacheDialog(this);
    wifiDialog = new WifiDialog(this, core->networkManager);
    screensaverDialog = new ScreensaverDialog(this);
    downloadMangaChaptersDialog = new DownloadMangaChaptersDialog(this);
    downloadStatusDialog = new DownloadStatusDialog(this);

    QObject::connect(menuDialog, &MenuDialog::finished,
                     [this](int b) { menuDialogButtonPressed(static_cast<MenuButton>(b)); });

    QObject::connect(clearCacheDialog, &MenuDialog::finished,
                     [this](int l) { core->clearDownloadCache(static_cast<ClearDownloadCacheLevel>(l)); });

    QObject::connect(updateMangaListsDialog, &UpdateMangaListsDialog::updateClicked, core,
                     &UltimateMangaReaderCore::updateMangaLists);

    QObject::connect(downloadMangaChaptersDialog, &DownloadMangaChaptersDialog::downloadConfirmed,
                     [this](auto m, auto f, auto t) {
                         downloadStatusDialog->open();
                         core->mangaChapterDownloadManager->downloadMangaChapters(m, f, t);
                     });

    QObject::connect(downloadStatusDialog, &DownloadStatusDialog::abortDownloads,
                     core->mangaChapterDownloadManager, &MangaChapterDownloadManager::cancelDownloads);

    // NetworkManager
    core->networkManager->setImageRescaleSize(this->size());

    QPixmap wifioff(":/images/icons/no-wifi.png");
    QPixmap wifion(":/images/icons/wifi.png");
    wifiIcons[0] = QIcon(wifioff.scaledToHeight(SIZES.wifiIconSize, Qt::SmoothTransformation));
    wifiIcons[1] = QIcon(wifion.scaledToHeight(SIZES.wifiIconSize, Qt::SmoothTransformation));

    QObject::connect(core->networkManager, &NetworkManager::connectionStatusChanged, [this](bool connected) {
        auto icon = connected ? wifiIcons[1] : wifiIcons[0];
        ui->toolButtonWifiIcon->setIcon(icon);
    });

    // MangaChapterDownloadManager
    QObject::connect(core->mangaChapterDownloadManager, &MangaChapterDownloadManager::error,
                     [this](auto msg) {
                         if (!core->settings.hideErrorMessages)
                             errorMessageWidget->showError(msg);
                     });

    QObject::connect(core->mangaChapterDownloadManager, &MangaChapterDownloadManager::downloadStart,
                     downloadStatusDialog, &DownloadStatusDialog::downloadStart);

    QObject::connect(core->mangaChapterDownloadManager,
                     &MangaChapterDownloadManager::downloadPagelistProgress, downloadStatusDialog,
                     &DownloadStatusDialog::downloadPagelistProgress);

    QObject::connect(core->mangaChapterDownloadManager, &MangaChapterDownloadManager::downloadPagesProgress,
                     downloadStatusDialog, &DownloadStatusDialog::downloadPagesProgress);

    QObject::connect(core->mangaChapterDownloadManager, &MangaChapterDownloadManager::downloadImagesProgress,
                     downloadStatusDialog, &DownloadStatusDialog::downloadImagesProgress);

    QObject::connect(core->mangaChapterDownloadManager, &MangaChapterDownloadManager::downloadCompleted,
                     downloadStatusDialog, &DownloadStatusDialog::downloadCompleted);

    // Core
    QObject::connect(core, &UltimateMangaReaderCore::error, [this](auto msg) {
        if (!core->settings.hideErrorMessages)
            errorMessageWidget->showError(msg);
    });

    QObject::connect(core, &UltimateMangaReaderCore::timeTick, this, &MainWidget::timerTick);

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

    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::downloadMangaClicked, [this]() {
        downloadMangaChaptersDialog->show(core->mangaController->currentManga,
                                          core->mangaController->currentIndex.chapter);
    });

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

void MainWidget::adjustUI()
{
    ui->pushButtonClose->setProperty("type", "borderless");
    ui->pushButtonFavorites->setProperty("type", "borderless");
    ui->pushButtonHome->setProperty("type", "borderless");

    ui->pushButtonClose->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonFavorites->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonHome->setFixedHeight(SIZES.buttonSize);

    ui->toolButtonMenu->setFixedSize(QSize(SIZES.menuIconSize, SIZES.menuIconSize));
    ui->toolButtonMenu->setIconSize(QSize(SIZES.menuIconSize, SIZES.menuIconSize));
    ui->toolButtonWifiIcon->setIconSize(QSize(SIZES.wifiIconSize, SIZES.wifiIconSize));

    ui->labelTitle->setStyleSheet("font-size: 18pt");
}

void MainWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    core->updateActiveScources();
    core->enableTimer(true);

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

void MainWidget::timerTick()
{
#ifdef KOBO
    // low battery guard
    if (KoboPlatformFunctions::getBatteryLevel() < 10)
        close();
#endif

    if (!suspendManager->sleeping)
    {
        ui->batteryIcon->updateIcon();
        ui->mangaReaderWidget->updateMenuBar();
    }
    else
    {
        if (core->networkManager->connected)
            core->networkManager->disconnectWifi();

        suspendManager->suspend(true);
    }
}

void MainWidget::onSuspend()
{
    screensaverDialog->showRandomScreensaver();

    disableFrontLight();

    core->networkManager->disconnectWifi();
}

void MainWidget::onResume()
{
    screensaverDialog->close();

    wifiDialog->connect();
    QTimer::singleShot(200, [this]() {
        setupFrontLight();
        if (!core->networkManager->connected)
            wifiDialog->open();
    });

    timerTick();
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

    core->networkManager->setImageRescaleSize(this->size());
}

void MainWidget::setWidgetTab(WidgetTab tab)
{
    enableVirtualKeyboard(false);
    core->mangaController->cancelAllPreloads();

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
            settingsDialog->open();
            break;
        case ClearDownloadsButton:
            clearCacheDialog->open();
            break;
        case UpdateMangaListsButton:
            updateMangaListsDialog->open();
            break;
    }
}

void MainWidget::on_toolButtonWifiIcon_clicked()
{
    if (!core->networkManager->checkInternetConnection())
        wifiDialog->open();
}
