#ifndef DOWNLOADMANGACHAPTERSDIALOG_H
#define DOWNLOADMANGACHAPTERSDIALOG_H

#include <mangainfo.h>

#include <QDialog>

namespace Ui
{
class DownloadMangaChaptersDialog;
}

class DownloadMangaChaptersDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadMangaChaptersDialog(QWidget *parent = nullptr);
    ~DownloadMangaChaptersDialog();

    void show(QSharedPointer<MangaInfo> mangaInfo, int chapterFrom = 0);

signals:
    void downloadConfirmed(QSharedPointer<MangaInfo> mangaInfo, int chapterFrom, int chapterTo);

protected:
    bool eventFilter(QObject *obj, QEvent *ev);

private slots:
    void on_pushButtonCancel_clicked();

    void on_pushButtonConfirm_clicked();

private:
    void adjustUI();

    Ui::DownloadMangaChaptersDialog *ui;

    QSharedPointer<MangaInfo> mangaInfo;
};

#endif  // DOWNLOADMANGACHAPTERSDIALOG_H
