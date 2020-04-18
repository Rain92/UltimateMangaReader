#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QScrollBar>
#include <QSplashScreen>
#include <QStandardItemModel>
#include <QStringListModel>

#include "clearcachedialog.h"
#include "downloadmanager.h"
#include "errormessagewidget.h"
#include "favoriteswidget.h"
#include "homewidget.h"
#include "mangareaderwidget.h"
#include "menudialog.h"
#include "screensaverdialog.h"
#include "settingsdialog.h"
#include "suspendmanager.h"
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
    void adjustSizes();
    void disableFrontLight();

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

    SuspendManager *suspendManager;
};

#endif  // MAINWIDGET_H
