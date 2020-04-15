#ifndef HOMEWIDGET_H
#define HOMEWIDGET_H

#include <QProgressDialog>
#include <QScrollBar>
#include <QStandardItemModel>

#include "abstractmangasource.h"
#include "clineedit.h"
#include "cscrollbar.h"
#include "sizes.h"
#include "staticsettings.h"
#include "ultimatemangareadercore.h"
#include "updatedialog.h"

namespace Ui
{
class HomeWidget;
}

class HomeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HomeWidget(QWidget *parent = 0);
    ~HomeWidget();

    void setCore(UltimateMangaReaderCore *core);

    void currentMangaSourceChanged();
    void updateSourcesList();

signals:
    void mangaSourceClicked(AbstractMangaSource *source);
    void mangaClicked(const QString &mangalink, const QString &mangatitle);
    void favoritesCleared();

private slots:
    void on_pushButtonUpdate_clicked();
    void on_listViewSources_clicked(const QModelIndex &index);
    void on_pushButtonFilter_clicked();
    void on_pushButtonFilterClear_clicked();
    void on_listViewMangas_clicked(const QModelIndex &index);
    void updateProgress(int progress);
    void updateError(const QString &error);

    void showEvent(QShowEvent *event) override;

private:
    Ui::HomeWidget *ui;

    UltimateMangaReaderCore *core;

    QMap<AbstractMangaSource *, int> sourcesprogress;

    QStringList filteredmangatitles;
    QStringList filteredmangalinks;
    bool filteractive;

    void refreshMangaListView();
    void adjustSizes();

    QList<QStandardItem *> listViewItemfromMangaSource(AbstractMangaSource *source);
    QProgressBar *updateProgressBar;
    UpdateDialog *updateDialog;
};

#endif  // HOMEWIDGET_H
