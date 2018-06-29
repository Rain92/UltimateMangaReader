#ifndef HOMEWIDGET_H
#define HOMEWIDGET_H

#include <QWidget>
#include <QStandardItem>
#include <QProgressBar>

#include <QStringListModel>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QProgressDialog>
#include <QDialogButtonBox>

#include "abstractmangasource.h"
#include "updatedialog.h"


namespace Ui {
class HomeWidget;
}

class HomeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HomeWidget(QWidget *parent = 0);
    ~HomeWidget();

    void setMangaSources(QList<AbstractMangaSource *> *sources);

signals:
    void mangaSourceClicked(AbstractMangaSource *source);
    void mangaClicked(const QString &mangalink, const QString &mangatitle);
    void favoritesCleared();
//    void favoritesInvalid();

private slots:
    void on_pushButtonUpdate_clicked();
    void on_pushButtonClearCache_clicked();
    void on_listViewSources_clicked(const QModelIndex &index);
    void on_pushButtonFilter_clicked();
    void on_pushButtonFilterClear_clicked();
    void on_listViewMangas_clicked(const QModelIndex &index);
    void updateProgress(int p);
    void updateError(const QString &error);
    void clearCacheDialogButtonClicked(QAbstractButton *button);

private:
    Ui::HomeWidget *ui;
    QVector<int> sourcesprogress;

    QList<AbstractMangaSource *> *mangasources;
    AbstractMangaSource *currentsource;

    QStringList filteredmangatitles;
    QStringList filteredmangalinks;


    void refreshMangaListView();
    void setupSourcesList();
    void adjustSizes();
    void setupClearCacheDialog();

    QList<QStandardItem *> *listViewItemfromMangaSource(AbstractMangaSource *source);
    QProgressBar *updateProgressBar;
    UpdateDialog *updatedialog;
    QDialogButtonBox *clearcachedialog;
};

#endif // HOMEWIDGET_H
