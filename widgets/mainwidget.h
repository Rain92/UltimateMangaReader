#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QStandardItemModel>
#include "favoritesmanager.h"
#include "downloadmanager.h"
#include "mangainfowidget.h"
#include "favoriteswidget.h"
#include "mangareaderwidget.h"
#include "homewidget.h"

#include "mangapanda.h"
#include "mangadex.h"
#include "mangatown.h"
#include "mangawindow.h"

#ifndef WINDOWS

#include "WidgetCommon.h"
#include "Config.h"
#include "QtUtils.h"
#include "Platform.h"
#include "FileOpenDialog.h"
#include "FileSaveDialog.h"
#include "WidgetCommon.h"
#include "VirtualKeyboard.h"
#include "VirtualKeyboardContainer.h"
#include "platform/KoboPlatform.h"

#endif

namespace Ui {
class MainWidget;
}

class MainWidget :
#ifdef WINDOWS
    public QWidget
#else
    public WidgetCommon
#endif
{
    Q_OBJECT

public:
    explicit MainWidget(QWidget *parent = 0);
    ~MainWidget();

public slots:

    void viewFavorite(Favorite fav, bool current);

    void setCurrentSource(AbstractMangaSource *source);
    void viewMangaInfo(const QString &mangalink, const QString &mangatitle);
    void toggleFavorite(MangaInfo *manga);

    void viewMangaImage(const MangaIndex &index);
    void advanceMangaPage(bool direction);
    void readerGoBack();

    void setWidgetTab(int page);

    void setFrontLight(int light, int comflight);

private slots:
    void on_pushButtonHome_clicked();
    void on_pushButtonClose_clicked();
    void on_pushButtonFavorites_clicked();



protected:
    void resizeEvent(QResizeEvent *event);

private:

    void setupUI();
    void setupFrontLight();
    void adjustSizes();
    void setupDirs();


    void setAddFavoriteButtonState(bool state);
    void updateReaderLabels();


    DownloadManager *downloadmanager;
    Ui::MainWidget *ui;

    void readerShowImage(const QString &path);


    QList<AbstractMangaSource *> mangasources;

    AbstractMangaSource *currentsource;
    MangaInfo *currentmanga;

    FavoritesManager favoritesmanager;

    int lastTab;

};

#endif // MAINWIDGET_H
