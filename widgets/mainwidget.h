#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QScrollBar>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QWidget>

#include "VirtualKeyboard.h"
#include "downloadmanager.h"
#include "favoriteswidget.h"
#include "homewidget.h"
#include "mangareaderwidget.h"
#include "ultimatemangareadercore.h"

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

    void restoreFrontLight();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    void setupVirtualKeyboard();
    void setupFrontLight();
    void adjustSizes();

    Ui::MainWidget *ui;

    UltimateMangaReaderCore *core;

    WidgetTab lastTab;

    QTimer restorefrontlighttimer;
    VirtualKeyboard *virtualKeyboard;
};

#endif  // MAINWIDGET_H
