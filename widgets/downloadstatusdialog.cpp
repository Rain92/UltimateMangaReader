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
    adjustUI();
}

void DownloadStatusDialog::adjustUI()
{
    ui->labelTitle->setStyleSheet("font-size: 13pt");
    ui->pushButtonCancel->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonOk->setFixedHeight(SIZES.buttonSize);
    this->setMinimumSize(SIZES.downloadStatusDialogWidth, SIZES.downloadStatusDialogHeight);
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

    ui->pushButtonCancel->show();
    ui->pushButtonOk->hide();
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
    ui->labelStatus->setText(QString("Downloading pages.\n%1 of %2.").arg(completed).arg(total));
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

    ui->pushButtonCancel->hide();
    ui->pushButtonOk->show();
}

void DownloadStatusDialog::checkFreeMem()
{
    auto freemem = getFreeSpace();
    ui->labelFreeMemory->setText(QString::number(freemem) + " MB");
    if (freemem < 200)
    {
        emit abortDownloads();

        ui->labelStep->setText("Error:");
        ui->labelStatus->setText("Storage memory low.");
    }
}

void DownloadStatusDialog::on_pushButtonOk_clicked()
{
    close();
}

void DownloadStatusDialog::on_pushButtonCancel_clicked()
{
    cancelled = true;
    emit abortDownloads();
    close();
}
