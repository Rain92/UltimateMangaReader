#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QScrollBar>
#include <QSplashScreen>
#include <QStandardItemModel>
#include <QStringListModel>

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

public slots:

    void readerGoBack();

    void setWidgetTab(WidgetTab tab);

    void setFrontLight(int light, int comflight);

    void enableVirtualKeyboard(bool enabled);

private slots:
    void on_pushButtonHome_clicked();
    void on_pushButtonClose_clicked();
    void on_pushButtonFavorites_clicked();

    void on_toolButtonMenu_clicked();

    void menuDialogButtonPressed(MenuButton button);

    void on_toolButtonWifiIcon_clicked();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;
    bool buttonPressEvent(QKeyEvent *event);
    bool buttonReleaseEvent(QKeyEvent *event);

private:
    void setupVirtualKeyboard();
    void setupFrontLight();
    void adjustUI();
    void disableFrontLight();

    void timerTick();
    void onSuspend();
    void onResume();

    Ui::MainWidget *ui;

    UltimateMangaReaderCore *core;

    WidgetTab lastTab;

    VirtualKeyboard *virtualKeyboard;
    ErrorMessageWidget *errorMessageWidget;
    QTimer *powerButtonTimer;

    MenuDialog *menuDialog;
    SettingsDialog *settingsDialog;
    UpdateMangaListsDialog *updateMangaListsDialog;
    ClearCacheDialog *clearCacheDialog;
    WifiDialog *wifiDialog;
    ScreensaverDialog *screensaverDialog;
    DownloadMangaChaptersDialog *downloadMangaChaptersDialog;
    DownloadStatusDialog *downloadStatusDialog;

    QIcon wifiIcons[2];
};

#endif  // MAINWIDGET_H
