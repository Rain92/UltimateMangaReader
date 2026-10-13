#include "wifidialog.h"

#include "ui_wifidialog.h"

WifiDialog::WifiDialog(QWidget *parent, NetworkManager *networkManager)
    : QDialog(parent), ui(new Ui::WifiDialog), networkManager(networkManager), awaitingResult(false)
{
    ui->setupUi(this);

    QObject::connect(networkManager, &NetworkManager::wifiConnectFinished, this, &WifiDialog::connectFinished);
}

WifiDialog::~WifiDialog()
{
    delete ui;
}

void WifiDialog::connect(bool showProgress)
{
    ui->labelInfo->setText("Connecting to WiFi...");
    ui->pushButtonRetry->hide();

    if (showProgress)
        open();

    awaitingResult = true;
    networkManager->connectWifiAsync();
}

void WifiDialog::cancel()
{
    awaitingResult = false;
    close();
}

void WifiDialog::connectFinished(bool connected)
{
    // the result of an attempt from before a suspend, or of an older one while a newer one runs
    if (!awaitingResult || networkManager->isConnectingWifi())
        return;
    awaitingResult = false;

    if (connected)
    {
        close();
        return;
    }

    ui->pushButtonRetry->show();
    ui->labelInfo->setText("Couldn't connect to WiFi.");
    open();
}

void WifiDialog::on_pushButtonRetry_clicked()
{
    connect(true);
}

void WifiDialog::on_pushButtonClose_clicked()
{
    close();
}
