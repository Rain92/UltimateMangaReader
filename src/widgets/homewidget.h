#pragma once

#include <QProgressDialog>
#include <QScrollBar>
#include <QStandardItemModel>

#include "clineedit.h"
#include "mangaliststore.h"
#include "mangasourcebase.h"
#include "sizes.h"
#include "staticsettings.h"
#include "ultimatemangareadercore.h"

namespace Ui
{
class HomeWidget;
}

class HomeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HomeWidget(QWidget *parent = nullptr);
    ~HomeWidget();

    // the lists of the sources, before a source is shown
    void setMangaLists(MangaListStore *lists) { mangaLists = lists; }
    void currentMangaSourceChanged(MangaSourceBase *source);
    // e.g. updated: the list is shown again if it is the one of the current source
    void mangaListChanged(MangaSourceBase *source);
    void updateSourcesList(const QList<MangaSourceBase *> &sources);

signals:
    void mangaSourceClicked(MangaSourceBase *source);
    // the filter wants the on-screen keyboard, or not anymore
    void virtualKeyboardRequested(bool show);
    void mangaClicked(const QString &mangaurl, const QString &mangatitle);

private slots:
    void on_listViewSources_clicked(const QModelIndex &index);
    void on_pushButtonFilter_clicked();
    void on_pushButtonFilterClear_clicked();
    void on_listViewMangas_clicked(const QModelIndex &index);

private:
    Ui::HomeWidget *ui;

    MangaListStore *mangaLists;
    MangaSourceBase *currentMangaSource;

    QStringList filteredMangaTitles;
    QStringList filteredMangaUrls;
    bool filteractive;

    // the one of the current source (loaded when it is needed)
    MangaList &currentList();
    void showMangaList();

    void refreshMangaListView();
    void adjustUI();

    QList<QStandardItem *> listViewItemfromMangaSource(MangaSourceBase *source);
    void updateMangaSourceSelection(int row);
};
