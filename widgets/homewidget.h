#ifndef HOMEWIDGET_H
#define HOMEWIDGET_H

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
    void updateSourcesList(const QList<AbstractMangaSource *> &sources);

signals:
    void mangaSourceClicked(AbstractMangaSource *source);
    void mangaClicked(const QString &mangalink, const QString &mangatitle);
    void favoritesCleared();

private slots:
    void on_listViewSources_clicked(const QModelIndex &index);
    void on_pushButtonFilter_clicked();
    void on_pushButtonFilterClear_clicked();
    void on_listViewMangas_clicked(const QModelIndex &index);

private:
    Ui::HomeWidget *ui;

    AbstractMangaSource *currentMangaSource;

    QStringList filteredMangaTitles;
    QStringList filteredMangaLinks;
    bool filteractive;

    void refreshMangaListView();
    void adjustSizes();

    QList<QStandardItem *> listViewItemfromMangaSource(AbstractMangaSource *source);
    void updateMangaSourceSelection(int row);
};

#endif  // HOMEWIDGET_H
