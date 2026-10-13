#include "downloadstatusdialog.h"

#include "ui_downloadstatusdialog.h"

DownloadStatusDialog::DownloadStatusDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::DownloadStatusDialog),
      pagelistDownloadErrors(0),
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
    ui->pushButtonRetry->setFixedHeight(SIZES.buttonSize);
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
    pagelistDownloadErrors = 0;
    pageDownloadErrors = 0;
    imageDownloadErrors = 0;
    updateErrors();
    cancelled = false;
    checkFreeMem();

    ui->pushButtonCancel->show();
    ui->pushButtonRetry->hide();
    ui->pushButtonOk->hide();
}

void DownloadStatusDialog::updateErrors()
{
    ui->labelDownloadErrors->setText(
        QString::number(pagelistDownloadErrors + pageDownloadErrors + imageDownloadErrors));
}

void DownloadStatusDialog::downloadPagelistProgress(int completed, int total, int errors)
{
    if (cancelled)
        return;

    ui->labelStep->setText("Step 1:");
    // one line: the form layout doesn't make room for a second one when the text changes
    ui->labelStatus->setText(QString("Page lists: %1 of %2").arg(completed).arg(total));
    pagelistDownloadErrors = errors;
    updateErrors();
    ui->progressBar->setValue(0 + 10.0 * completed / total);
    checkFreeMem();
}

void DownloadStatusDialog::downloadPagesProgress(int completed, int total, int errors)
{
    if (cancelled)
        return;

    ui->labelStep->setText("Step 2:");
    ui->labelStatus->setText(QString("Page details: %1 of %2").arg(completed).arg(total));
    pageDownloadErrors = errors;
    updateErrors();
    ui->progressBar->setValue(10 + 10.0 * completed / total);
}

void DownloadStatusDialog::downloadImagesProgress(int completed, int total, int errors)
{
    if (cancelled)
        return;

    ui->labelStep->setText("Step 3:");
    ui->labelStatus->setText(QString("Pages: %1 of %2").arg(completed).arg(total));
    imageDownloadErrors = errors;
    updateErrors();
    ui->progressBar->setValue(20 + 80.0 * completed / total);
    checkFreeMem();
}

void DownloadStatusDialog::downloadCompleted(int failedChapters)
{
    if (cancelled)
        return;

    ui->labelStep->setText("Completed!");
    // one line (see downloadPagelistProgress())
    ui->labelStatus->setText(failedChapters == 0   ? ""
                             : failedChapters == 1 ? "1 chapter incomplete"
                                                   : QString("%1 chapters incomplete").arg(failedChapters));
    ui->progressBar->setValue(100);

    int errors = pagelistDownloadErrors + pageDownloadErrors + imageDownloadErrors;

    QString msg(errors == 0 ? QString("Download of %1 completed!").arg(ui->labelMangaTitle->text())
                            : QString("Download of %1 completed with %2 errors!")
                                  .arg(ui->labelMangaTitle->text())
                                  .arg(errors));

    qDebug() << msg;

    ui->pushButtonCancel->hide();
    ui->pushButtonRetry->setVisible(failedChapters > 0);
    ui->pushButtonOk->show();
}

void DownloadStatusDialog::checkFreeMem()
{
    auto freemem = getFreeSpace();
    ui->labelFreeMemory->setText(QString::number(freemem) + " MB");
    if (freemem < 200 && !cancelled)
    {
        // the downloads that already run still report their progress and the completion
        cancelled = true;
        emit abortDownloads();

        ui->labelStep->setText("Error:");
        ui->labelStatus->setText("Storage memory low.");
        ui->pushButtonCancel->hide();
        ui->pushButtonOk->show();
    }
}

void DownloadStatusDialog::on_pushButtonOk_clicked()
{
    close();
}

void DownloadStatusDialog::on_pushButtonRetry_clicked()
{
    ui->pushButtonRetry->hide();
    emit retryFailed();
}

void DownloadStatusDialog::on_pushButtonCancel_clicked()
{
    cancelled = true;
    emit abortDownloads();
    close();
}
