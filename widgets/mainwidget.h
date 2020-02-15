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
#include "settings.h"

#include "mangapanda.h"
#include "mangadex.h"
#include "mangatown.h"
#include "mangawindow.h"
#include "jaiminisbox.h"

namespace Ui {
class MainWidget;
}

class MainWidget :
    public QWidget
{
    Q_OBJECT

public:
    explicit MainWidget(QWidget *parent = 0);
    ~MainWidget();

public slots:

    void viewFavorite(QSharedPointer<MangaInfo> info, bool current);

    void setCurrentSource(AbstractMangaSource *source);
    void viewMangaInfo(QSharedPointer<MangaInfo> info);
    void viewMangaInfo(const QString &mangalink, const QString &mangatitle);
    void toggleFavorite(QSharedPointer<MangaInfo> manga);

    void viewMangaImage(const MangaIndex &index);
    void advanceMangaPage(bool direction);
    void readerGoBack();

    void setWidgetTab(int page);

    void setFrontLight(int light, int comflight);

    void enableVirtualKeyboard(bool enabled);

private slots:
    void on_pushButtonHome_clicked();
    void on_pushButtonClose_clicked();
    void on_pushButtonFavorites_clicked();

    void resetCloseCounter();

    void restoreFrontLight();

protected:
    void resizeEvent(QResizeEvent *event);

private:

    void setupUI();
    void setupFrontLight();
    void adjustSizes();
    void setupDirs();

    void setAddFavoriteButtonState(bool state);
    void updateReaderLabels();

    void readerShowImage(const QString &path);

    DownloadManager *downloadmanager;
    Ui::MainWidget *ui;

    QList<AbstractMangaSource *> mangasources;

    AbstractMangaSource *currentsource;
    QSharedPointer<MangaInfo> currentmanga;

    FavoritesManager favoritesmanager;
    Settings settings;

    int lastTab;

    int closecounter;
    QTimer closecounterresettimer;
    QTimer restorefrontlighttimer;
};

#endif // MAINWIDGET_H
