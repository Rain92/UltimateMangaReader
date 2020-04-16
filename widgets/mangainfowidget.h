#ifndef MANGAINFOWIDGET_H
#define MANGAINFOWIDGET_H

#include <QIcon>
#include <QWidget>

#include "mangaindex.h"
#include "mangainfo.h"
#include "sizes.h"
#include "utils.h"

namespace Ui
{
class MangaInfoWidget;
}

class MangaInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MangaInfoWidget(QWidget *parent = 0);
    ~MangaInfoWidget();

    void setManga(QSharedPointer<MangaInfo> manga);
    void setFavoriteButtonState(bool state);

signals:
    void toggleFavoriteClicked(QSharedPointer<MangaInfo> manga);
    void readMangaClicked(const MangaIndex &index);
    void readMangaContinueClicked();

public slots:
    void updateManga(bool newchapters);
    void updateCover();

private slots:
    void on_toolButtonAddFavorites_clicked();

    void on_listViewChapters_clicked(const QModelIndex &index);

    void on_pushButtonReadLatest_clicked();
    void on_pushButtonReadContinue_clicked();
    void on_pushButtonReadFirst_clicked();

private:
    Ui::MangaInfoWidget *ui;

    QSharedPointer<MangaInfo> currentmanga;

    void adjustSizes();

    void updateInfos();
};

#endif  // MANGAINFOWIDGET_H
