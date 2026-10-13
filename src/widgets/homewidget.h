#pragma once

#include <QProgressDialog>
#include <QScrollBar>
#include <QStandardItemModel>

#include "abstractmangasource.h"
#include "clineedit.h"
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

    void currentMangaSourceChanged(AbstractMangaSource *source);
    // e.g. updated: the list is shown again if it is the one of the current source
    void mangaListChanged(AbstractMangaSource *source);
    void updateSourcesList(const QList<AbstractMangaSource *> &sources);

signals:
    void mangaSourceClicked(AbstractMangaSource *source);
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

    AbstractMangaSource *currentMangaSource;

    QStringList filteredMangaTitles;
    QStringList filteredMangaUrls;
    bool filteractive;

    void showMangaList();
    void refreshMangaListView();
    void adjustUI();

    QList<QStandardItem *> listViewItemfromMangaSource(AbstractMangaSource *source);
    void updateMangaSourceSelection(int row);
};
