#include "wifidialog.h"

#include "ui_wifidialog.h"

WifiDialog::WifiDialog(QWidget *parent, DownloadManager *downloadManager)
    : QDialog(parent), ui(new Ui::WifiDialog), downloadManager(downloadManager)
{
    ui->setupUi(this);
}

WifiDialog::~WifiDialog()
{
    delete ui;
}

void WifiDialog::connect()
{
    ui->labelInfo->setText("Connecting to WiFi...");
    ui->pushButtonRetry->hide();

    QtConcurrent::run([this]() {
        downloadManager->connect();
        if (downloadManager->connected)
        {
            close();
        }
        else
        {
            ui->pushButtonRetry->show();
            ui->labelInfo->setText("Couldn't connect to WiFi.");
        }
    });
}

void WifiDialog::on_pushButtonRetry_clicked()
{
    connect();
}

void WifiDialog::on_pushButtonClose_clicked()
{
    close();
}
