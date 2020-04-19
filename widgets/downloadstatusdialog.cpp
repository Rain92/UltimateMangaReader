#include "downloadstatusdialog.h"

#include "ui_downloadstatusdialog.h"

DownloadStatusDialog::DownloadStatusDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::DownloadStatusDialog),
      pageDownloadErrors(0),
      imageDownloadErrors(0),
      cancelled(false)
{
    ui->setupUi(this);
    adjustSizes();

#ifdef KOBO
    // disabe background download
    ui->pushButtonHide->hide();
#endif
}

void DownloadStatusDialog::adjustSizes()
{
    ui->labelTitle->setStyleSheet("font-size: 13pt");
    ui->pushButtonCancel->setFixedHeight(buttonsize);
    ui->pushButtonHide->setFixedHeight(buttonsize);
    this->setMinimumSize(mm_to_px(80), mm_to_px(60));
}

DownloadStatusDialog::~DownloadStatusDialog()
{
    delete ui;
}

void DownloadStatusDialog::downloadStart(const QString &mangaTitle)
{
    ui->labelMangaTitle->setText(mangaTitle);
    ui->labelStep->setText("");
    ui->labelStatus->setText("");
    ui->progressBar->setValue(0);
    pageDownloadErrors = 0;
    imageDownloadErrors = 0;
    cancelled = false;
    checkFreeMem();
}

void DownloadStatusDialog::downloadPagelistProgress(int completed, int total)
{
    ui->labelStep->setText("Step 1:");
    ui->labelStatus->setText(QString("Downloading pagelists.\nChapter %1 of %2.").arg(completed).arg(total));
    ui->progressBar->setValue(0 + 10.0 * completed / total);
    checkFreeMem();
}

void DownloadStatusDialog::downloadPagesProgress(int completed, int total, int errors)
{
    ui->labelStep->setText("Step 2:");
    ui->labelStatus->setText(
        QString("Downloading page details.\nChapter %1 of %2.").arg(completed).arg(total));
    pageDownloadErrors = errors;
    ui->labelDownloadErrors->setText(QString::number(pageDownloadErrors + imageDownloadErrors));
    ui->progressBar->setValue(10 + 10.0 * completed / total);
}

void DownloadStatusDialog::downloadImagesProgress(int completed, int total, int errors)
{
    ui->labelStep->setText("Step 3:");
    ui->labelStatus->setText(QString("Downloading images.\n%1 of %2.").arg(completed).arg(total));
    imageDownloadErrors = errors;
    ui->labelDownloadErrors->setText(QString::number(pageDownloadErrors + imageDownloadErrors));
    ui->progressBar->setValue(20 + 80.0 * completed / total);
    checkFreeMem();
}

void DownloadStatusDialog::downloadCompleted()
{
    if (cancelled)
        return;

    ui->labelStep->setText("Completed!");
    ui->labelStatus->setText("");

    int errors = pageDownloadErrors + imageDownloadErrors;

    QString msg(errors == 0 ? QString("Download of %1 completed!").arg(ui->labelMangaTitle->text())
                            : QString("Download of %1 completed with %2 errors!")
                                  .arg(ui->labelMangaTitle->text())
                                  .arg(pageDownloadErrors + imageDownloadErrors));

    QMessageBox msgBox;
    msgBox.setText(msg);
    msgBox.setModal(false);
    msgBox.exec();
    close();
}

void DownloadStatusDialog::checkFreeMem()
{
    long freemem = getFreeSpace();
    ui->labelFreeMemory->setText(QString::number(freemem));
    if (freemem < 200)
    {
        emit abortDownloads();

        ui->labelStep->setText("Error:");
        ui->labelStatus->setText("Storage memory low.");

        QMessageBox msgBox;
        msgBox.setText("Storage memory low!\nAborting downloads.");
        msgBox.setModal(false);
        msgBox.exec();
    }
}

void DownloadStatusDialog::on_pushButtonHide_clicked()
{
    close();
}

void DownloadStatusDialog::on_pushButtonCancel_clicked()
{
    cancelled = true;
    emit abortDownloads();
    close();
}
