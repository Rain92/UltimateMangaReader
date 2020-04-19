#ifndef DOWNLOADSTATUSDIALOG_H
#define DOWNLOADSTATUSDIALOG_H

#include <QDialog>
#include <QMessageBox>

#include "sizes.h"
#include "utils.h"

namespace Ui
{
class DownloadStatusDialog;
}

class DownloadStatusDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadStatusDialog(QWidget *parent = nullptr);
    ~DownloadStatusDialog();

    void downloadStart(const QString &mangaTitle);
    void downloadPagelistProgress(int completed, int total);
    void downloadPagesProgress(int completed, int total, int errors);
    void downloadImagesProgress(int completed, int total, int errors);
    void downloadCompleted();

signals:
    void abortDownloads();

private slots:
    void on_pushButtonHide_clicked();

    void on_pushButtonCancel_clicked();

private:
    Ui::DownloadStatusDialog *ui;
    void adjustSizes();
    void checkFreeMem();
    int pageDownloadErrors;
    int imageDownloadErrors;
    bool cancelled;
};

#endif  // DOWNLOADSTATUSDIALOG_H
