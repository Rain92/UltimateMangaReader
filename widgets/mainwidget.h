#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QStandardItemModel>
#include "favoritesmanager.h"
#include "downloadmanager.h"
#include "mangapanda.h"
#include "mangadex.h"
#include "mangatown.h"
#include "homewidget.h"
#include "mangainfowidget.h"
#include "favoriteswidget.h"
#include "mangareaderwidget.h"


#ifdef KOBO
#include "WidgetCommon.h"
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

    void setFrontLight();

private slots:
    void on_pushButtonHome_clicked();
    void on_pushButtonClose_clicked();
    void on_pushButtonFavorites_clicked();



protected:
    void resizeEvent(QResizeEvent *event);

private:

    void setupUI();
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

    FavoritesManager favorites;

    int lastTab;
};

#endif // MAINWIDGET_H
