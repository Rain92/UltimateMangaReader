#include "mainwidget.h"

#include "ui_mainwidget.h"
#include "widgetutils.h"

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::MainWidget),
      core(new UltimateMangaReaderCore(this)),
      lastTab(MangaInfoTab),
      virtualKeyboard(new VirtualKeyboard(this)),
      errorMessageWidget(new ErrorMessageWidget(this)),
      powerButtonTimer(new QTimer(this))
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
    aboutDialog = new AboutDialog(this);

    // the hardware keys (power button, sleep cover) go to the open dialog, from where they reach the main widget
    for (QDialog *dialog : {static_cast<QDialog *>(updateMangaListsDialog), static_cast<QDialog *>(wifiDialog),
                            static_cast<QDialog *>(downloadMangaChaptersDialog),
                            static_cast<QDialog *>(downloadStatusDialog), static_cast<QDialog *>(menuDialog),
                            static_cast<QDialog *>(settingsDialog), static_cast<QDialog *>(clearCacheDialog),
                            static_cast<QDialog *>(aboutDialog), ui->mangaReaderWidget->gotoDialog()})
        dialog->installEventFilter(this);

    QObject::connect(menuDialog, &MenuDialog::finished, this,
                     [this](int b) { menuDialogButtonPressed(static_cast<MenuButton>(b)); });

    // closing it (Cancel, a tap outside) finishes it with 0
    QObject::connect(clearCacheDialog, &ClearCacheDialog::finished, this,
                     [this](int l)
                     {
                         if (l >= ClearImages)
                             core->clearDownloadCache(static_cast<ClearDownloadCacheLevel>(l));
                     });

    QObject::connect(updateMangaListsDialog, &UpdateMangaListsDialog::updateClicked, core->mangaListUpdater,
                     &MangaListUpdater::updateMangaLists);

    QObject::connect(downloadMangaChaptersDialog, &DownloadMangaChaptersDialog::downloadConfirmed, this,
                     [this](auto m, auto f, auto t)
                     {
                         downloadStatusDialog->open();
                         core->mangaChapterDownloadManager->downloadMangaChapters(m, f, t);
                     });

    QObject::connect(downloadStatusDialog, &DownloadStatusDialog::abortDownloads,
                     core->mangaChapterDownloadManager, &MangaChapterDownloadManager::cancelDownloads);

    QObject::connect(downloadStatusDialog, &DownloadStatusDialog::retryFailed, core->mangaChapterDownloadManager,
                     &MangaChapterDownloadManager::retryFailedDownloads);

    // NetworkManager
    updatePageSize();

    QPixmap wifioff(":/images/icons/no-wifi.png");
    QPixmap wifion(":/images/icons/wifi.png");
    wifiIcons[0] = QIcon(
        wifioff.scaledToHeight(SIZES.wifiIconSize * qApp->devicePixelRatio(), Qt::SmoothTransformation));
    wifiIcons[1] =
        QIcon(wifion.scaledToHeight(SIZES.wifiIconSize * qApp->devicePixelRatio(), Qt::SmoothTransformation));

    // emitted on the WiFi thread
    QObject::connect(core->networkManager, &NetworkManager::connectionStatusChanged, this,
                     [this](bool connected)
                     {
                         auto icon = connected ? wifiIcons[1] : wifiIcons[0];
                         ui->toolButtonWifiIcon->setIcon(icon);
                     });

    // MangaChapterDownloadManager
    QObject::connect(core->mangaChapterDownloadManager, &MangaChapterDownloadManager::error, this,
                     &MainWidget::showErrorMessage);

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
    QObject::connect(core, &UltimateMangaReaderCore::error, this, &MainWidget::showErrorMessage);

    QObject::connect(core, &UltimateMangaReaderCore::timeTick, this, &MainWidget::timerTick);

    QObject::connect(core->sources, &SourceRegistry::activeSourcesChanged, ui->homeWidget,
                     &HomeWidget::updateSourcesList);

    QObject::connect(core, &UltimateMangaReaderCore::downloadCacheCleared, this,
                     [this]()
                     {
                         ui->mangaReaderWidget->clearCache();
                         // clearing can remove favorites
                         if (ui->stackedWidget->currentIndex() == FavoritesTab)
                             ui->favoritesWidget->showFavoritesList();
                         if (auto manga = core->mangaController->currentManga)
                             ui->mangaInfoWidget->setFavoriteButtonState(core->favoritesManager->isFavorite(manga));
                     });

    // MangaController
    QObject::connect(core->mangaController, &MangaController::currentMangaChanged, this,
                     [this](auto info)
                     {
                         ui->mangaInfoWidget->setManga(info);
                         bool state = core->favoritesManager->isFavorite(info);
                         ui->mangaInfoWidget->setFavoriteButtonState(state);
                         setWidgetTab(MangaInfoTab);
                         ui->mangaReaderWidget->clearCache();
                     });

    QObject::connect(core->mangaController, &MangaController::completedImagePreloadSignal,
                     ui->mangaReaderWidget, &MangaReaderWidget::prefetchImage);

    QObject::connect(core->mangaController, &MangaController::currentIndexChanged, ui->mangaReaderWidget,
                     &MangaReaderWidget::updateCurrentIndex);

    QObject::connect(core->mangaController, &MangaController::currentImageChanged, ui->mangaReaderWidget,
                     &MangaReaderWidget::showImage);

    QObject::connect(core->mangaController, &MangaController::indexMovedOutOfBounds, this,
                     &MainWidget::readerGoBack);

    QObject::connect(core->mangaController, &MangaController::error, this, &MainWidget::showErrorMessage);

    // FavoritesManager
    QObject::connect(core->favoritesManager, &FavoritesManager::error, this, &MainWidget::showErrorMessage);

    // HomeWidget
    QObject::connect(ui->homeWidget, &HomeWidget::mangaSourceClicked, core,
                     [this](AbstractMangaSource *source)
                     {
                         core->sources->setCurrent(source);
                         core->activity();
                     });

    QObject::connect(ui->homeWidget, &HomeWidget::mangaClicked, core,
                     &UltimateMangaReaderCore::setCurrentManga);

    QObject::connect(core->sources, &SourceRegistry::currentSourceChanged, ui->homeWidget,
                     &HomeWidget::currentMangaSourceChanged);

    QObject::connect(core->mangaListUpdater, &MangaListUpdater::mangaListChanged, ui->homeWidget,
                     &HomeWidget::mangaListChanged);

    // MangaInfoWidget
    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::toggleFavoriteClicked, this,
                     [this](auto info)
                     {
                         bool newstate = core->favoritesManager->toggleFavorite(info);
                         ui->mangaInfoWidget->setFavoriteButtonState(newstate);
                     });

    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::readMangaClicked, this,
                     [this](auto index)
                     {
                         setWidgetTab(MangaReaderTab);
                         core->mangaController->setCurrentIndex(index);
                         QTimer::singleShot(50, core->mangaController, &MangaController::preloadNeighbours);
                     });

    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::readMangaContinueClicked, this,
                     [this]() { setWidgetTab(MangaReaderTab); });

    QObject::connect(ui->mangaInfoWidget, &MangaInfoWidget::downloadMangaClicked, this,
                     [this]()
                     {
                         downloadMangaChaptersDialog->show(core->mangaController->currentManga,
                                                           core->mangaController->currentIndex.chapter);
                     });

    // FavoritesWidget
    ui->favoritesWidget->setFavoritesManager(core->favoritesManager);

    QObject::connect(ui->favoritesWidget, &FavoritesWidget::favoriteClicked, this,
                     [this](auto mangainfo, auto jumptoreader)
                     {
                         // the info stays on the screen if the reader can't show it (no chapters)
                         if (core->mangaController->setCurrentManga(mangainfo) && jumptoreader)
                             setWidgetTab(MangaReaderTab);
                     });

    QObject::connect(ui->favoritesWidget, &FavoritesWidget::error, this, &MainWidget::showErrorMessage);


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

    // the pages of chapters that are downloaded for later would only push the ones around the current page out
    QObject::connect(core->networkManager, &NetworkManager::downloadedImage, ui->mangaReaderWidget,
                     [this](const QString &path, QSharedPointer<QImage> img)
                     {
                         auto manga = core->mangaController->currentManga;
                         if (manga && path.startsWith(CONF.mangaimagesdir(manga->hostname, manga->url)))
                             ui->mangaReaderWidget->addImageToCache(path, img);
                     });

    // SettingsDialog
    QObject::connect(settingsDialog, &SettingsDialog::activeMangasChanged, core,
                     &UltimateMangaReaderCore::updateActiveSources);

    QObject::connect(settingsDialog, &SettingsDialog::mangaOrderMethodChanged, core->mangaListUpdater,
                     &MangaListUpdater::sortMangaLists);

    QObject::connect(settingsDialog, &SettingsDialog::ditheringMethodChanged, this,
                     &MainWidget::updateDitheringMode);

    // SuspendManager

    QObject::connect(core->suspendManager, &SuspendManager::suspending, this, &MainWidget::onSuspend);
    QObject::connect(core->suspendManager, &SuspendManager::resuming, this, &MainWidget::onResume);
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

    ui->labelTitle->setStyleSheet("font-size: 16pt");

#ifdef KOBO
    koboDevice = KoboPlatformFunctions::getKoboDeviceDescriptor();
    this->resize(koboDevice.width, koboDevice.height);
#endif
}

void MainWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    core->updateActiveSources();
    updateDitheringMode();

    QTimer::singleShot(500, this, &MainWidget::onResume);

    // UMR_OPEN_FAVORITE=<n> opens the n-th favorite in the reader right away, -1 the favorites list (for
    // profiling over ssh, where the screen can't be tapped)
    bool ok = false;
    int favoriteIndex = qEnvironmentVariableIntValue("UMR_OPEN_FAVORITE", &ok);
    if (ok)
        QTimer::singleShot(1500, this,
                           [this, favoriteIndex]()
                           {
                               if (favoriteIndex == -1)
                               {
                                   setWidgetTab(FavoritesTab);
                                   return;
                               }

                               core->favoritesManager->loadInfos();
                               auto info = core->favoritesManager->info(favoriteIndex);
                               if (!info)
                               {
                                   qDebug() << "UMR_OPEN_FAVORITE: no favorite" << favoriteIndex;
                                   return;
                               }
                               core->mangaController->setCurrentManga(info);
                               setWidgetTab(MangaReaderTab);
                           });
}

bool MainWidget::event(QEvent *event)
{
    // e.g. the fractional scale of a Wayland window, which only arrives after it is shown
    if (event->type() == QEvent::DevicePixelRatioChange)
        updatePageSize();

    if (event->type() == QEvent::KeyPress)
        return buttonPressEvent(static_cast<QKeyEvent *>(event));
    else if (event->type() == QEvent::KeyRelease)
        return buttonReleaseEvent(static_cast<QKeyEvent *>(event));
    return QWidget::event(event);
}

bool MainWidget::buttonReleaseEvent(QKeyEvent *event)
{
    if (event->key() == POWERBUTTON)
    {
        qDebug() << "Powerkey release";
        powerButtonTimer->stop();

        if (!core->suspendManager->sleeping)
            core->suspendManager->suspend();
        else
            core->suspendManager->resume();
    }
    else if (event->key() == SLEEPCOVERBUTTON)
    {
        qDebug() << "Sleepcover opened";
        core->suspendManager->resume();

        return true;
    }

    return false;
}

bool MainWidget::buttonPressEvent(QKeyEvent *event)
{
    // the repeats of a held button: the power button's timer must run on, the pages would fly by
    if (event->isAutoRepeat())
        return true;

    if (event->key() == POWERBUTTON)
    {
        qDebug() << "Powerkey press";
        powerButtonTimer->start(2000);

        return true;
    }
    else if (event->key() == SLEEPCOVERBUTTON)
    {
        qDebug() << "Sleepcover closed";
        core->suspendManager->suspend();

        return true;
    }

    return false;
}

void MainWidget::timerTick()
{
#ifdef KOBO
    // low battery guard
    if (KoboPlatformFunctions::getBatteryLevel() < 10 && !KoboPlatformFunctions::isBatteryCharging())
        close();
#endif

    ui->batteryIcon->updateIcon();
    ui->mangaReaderWidget->updateMenuBar();
}

void MainWidget::onSuspend()
{
    core->enableTimers(false);
    wifiDialog->cancel();
    screensaverDialog->showRandomScreensaver();

    disableFrontLight();

    core->networkManager->disconnectWifi();
}

void MainWidget::onResume()
{
    screensaverDialog->close();
    core->enableTimers(true);

    // in the background, connecting takes seconds when the WiFi was off. The dialog only shows up if it fails.
    wifiDialog->connect();
    QTimer::singleShot(200, this, &MainWidget::setupFrontLight);
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

void MainWidget::updateDitheringMode()
{
#ifdef KOBO
    if (core->settings.ditheringMode == NoDithering)
        KoboPlatformFunctions::enableDithering(false, false);
    else
        KoboPlatformFunctions::enableDithering(true, false);
#endif
}

void MainWidget::setupFrontLight()
{
    setFrontLight(core->settings.lightValue, core->settings.comflightValue);

#ifdef KOBO
    ui->mangaReaderWidget->setFrontLightPanelState(
        koboDevice.frontlightSettings.frontlightMin, koboDevice.frontlightSettings.frontlightMax,
        core->settings.lightValue, koboDevice.frontlightSettings.naturalLightMin,
        koboDevice.frontlightSettings.naturalLightMax, core->settings.comflightValue);
#endif
}

void MainWidget::setFrontLight(int light, int comflight)
{
#ifdef KOBO
    if (!koboDevice.frontlightSettings.hasFrontLight)
        return;

    // only the hardware takes the inverted value, the settings keep the slider's (it flipped with every call)
    int hardwareComflight = koboDevice.frontlightSettings.naturalLightInverted
                                ? koboDevice.frontlightSettings.naturalLightMax - comflight
                                : comflight;
    KoboPlatformFunctions::setFrontlightLevel(light, hardwareComflight);
#endif

    if (core->settings.lightValue != light || core->settings.comflightValue != comflight)
    {
        core->settings.lightValue = light;
        core->settings.comflightValue = comflight;

        core->settings.scheduleSerialize();
    }
}

void MainWidget::showErrorMessage(const QString &message)
{
    if (core->settings.hideErrorMessages)
        return;

    errorMessageWidget->showError(message);
    qDebug() << "Error occured:" << message;
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

    updatePageSize();
}

// The pages are rendered for the screen (in device pixels) when they are downloaded.
void MainWidget::updatePageSize()
{
    setWindowPixelRatio(devicePixelRatioF());

#ifdef KOBO
    core->networkManager->setDownloadSettings({koboDevice.width, koboDevice.height}, &core->settings);
#else
    core->networkManager->setDownloadSettings(this->size() * devicePixelRatioF(), &core->settings);
#endif
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
            // the preloads were cancelled above, the neighbours of the page are wanted now
            QTimer::singleShot(50, core->mangaController, &MangaController::preloadNeighbours);
            break;
    }
    ui->frameHeader->repaint();

    ui->batteryIcon->updateIcon();
    ui->stackedWidget->setCurrentIndex(tab);

    // kiwilex issue-15
    // force focus on widget to activate signal for hardware buttons
    QWidget *w = ui->stackedWidget->currentWidget();
    if (w)
        w->setFocus();
}

void MainWidget::readerGoBack()
{
    setWidgetTab(lastTab);
}

bool MainWidget::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::RequestSoftwareInputPanel + 1000)
    {
        enableVirtualKeyboard(true);
        return true;
    }
    else if (event->type() == QEvent::CloseSoftwareInputPanel + 1000)
    {
        enableVirtualKeyboard(false);
        return true;
    }

    if (event->type() == QEvent::KeyPress)
        return buttonPressEvent(static_cast<QKeyEvent *>(event));
    else if (event->type() == QEvent::KeyRelease)
        return buttonReleaseEvent(static_cast<QKeyEvent *>(event));

    return false;
}

void MainWidget::on_toolButtonMenu_clicked()
{
    menuDialog->move(this->mapToGlobal(QPoint{0, 0}));
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
        case AboutButton:
            aboutDialog->open();
            break;
    }
}

void MainWidget::on_toolButtonWifiIcon_clicked()
{
    // checks the connection in the background, while connected the dialog only shows up if it got lost
    wifiDialog->connect(!core->networkManager->connected);
}
