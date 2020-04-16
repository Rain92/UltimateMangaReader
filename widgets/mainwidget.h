#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QScrollBar>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QWidget>

#include "clearcachedialog.h"
#include "downloadmanager.h"
#include "errormessagewidget.h"
#include "favoriteswidget.h"
#include "homewidget.h"
#include "mangareaderwidget.h"
#include "menudialog.h"
#include "settingsdialog.h"
#include "ultimatemangareadercore.h"
#include "updatemangalistsdialog.h"
#include "virtualkeyboard.h"

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

    void restoreFrontLight();

    void menuDialogButtonPressed(MenuButton button);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void showEvent(QShowEvent *event) override;

private:
    void setupVirtualKeyboard();
    void setupFrontLight();
    void adjustSizes();

    Ui::MainWidget *ui;

    UltimateMangaReaderCore *core;

    WidgetTab lastTab;

    QTimer restorefrontlighttimer;
    VirtualKeyboard *virtualKeyboard;
    ErrorMessageWidget *errorMessageWidget;

    MenuDialog *menuDialog;
    SettingsDialog *settingsDialog;
    UpdateMangaListsDialog *updateMangaListsDialog;
    ClearCacheDialog *clearCacheDialog;
};

#endif  // MAINWIDGET_H
