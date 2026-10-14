#pragma once

#include <QScrollBar>
#include <QSplashScreen>
#include <QStandardItemModel>
#include <QStringListModel>

#include "aboutdialog.h"
#include "clearcachedialog.h"
#include "downloadmangachaptersdialog.h"
#include "downloadstatusdialog.h"
#include "errormessagewidget.h"
#include "favoriteswidget.h"
#include "homewidget.h"
#include "mangareaderwidget.h"
#include "menudialog.h"
#include "networkmanager.h"
#include "screensaverdialog.h"
#include "settingsdialog.h"
#include "sourceupdatedialog.h"
#include "ultimatemangareadercore.h"
#include "updatemangalistsdialog.h"
#include "virtualkeyboard.h"
#include "wifidialog.h"

namespace Ui
{
class MainWidget;
}

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

public:
    void readerGoBack();

    void setWidgetTab(WidgetTab tab);

    void setFrontLight(int light, int comflight);

    void enableVirtualKeyboard(bool enabled);

    void showErrorMessage(const QString &message);

private slots:
    void on_pushButtonHome_clicked();
    void on_pushButtonClose_clicked();
    void on_pushButtonFavorites_clicked();

    void on_toolButtonMenu_clicked();

    void menuDialogButtonPressed(MenuButton button);

    void on_toolButtonWifiIcon_clicked();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;
    bool buttonPressEvent(QKeyEvent *event);
    bool buttonReleaseEvent(QKeyEvent *event);

private:
    void setupVirtualKeyboard();
    void setupFrontLight();
    void adjustUI();
    void updatePageSize();
    void disableFrontLight();

    void updateDitheringMode();

    void timerTick();
    void onSuspend();
    void onResume();

    // Asks the repository for updates of the sources, once (again after a network error), when the app is online.
    Task<> checkSourceUpdates();
    // Asks whether to install the update that was found, when nothing else is shown (not while reading).
    void offerSourceUpdate();

    Ui::MainWidget *ui;

    UltimateMangaReaderCore *core;

    WidgetTab lastTab;

    VirtualKeyboard *virtualKeyboard;
    ErrorMessageWidget *errorMessageWidget;
    QTimer *powerButtonTimer;

    MenuDialog *menuDialog;
    SettingsDialog *settingsDialog;
    UpdateMangaListsDialog *updateMangaListsDialog;
    AboutDialog *aboutDialog;
    ClearCacheDialog *clearCacheDialog;
    WifiDialog *wifiDialog;
    ScreensaverDialog *screensaverDialog;
    DownloadMangaChaptersDialog *downloadMangaChaptersDialog;
    DownloadStatusDialog *downloadStatusDialog;
    SourceUpdateDialog *sourceUpdateDialog;

    bool sourceUpdatesChecked;
    std::optional<SourceUpdater::Changes> pendingSourceUpdate;

    QIcon wifiIcons[2];
};
