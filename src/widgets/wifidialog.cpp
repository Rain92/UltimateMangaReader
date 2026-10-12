#include "wifidialog.h"

#include "ui_wifidialog.h"

WifiDialog::WifiDialog(QWidget *parent, NetworkManager *networkManager)
    : QDialog(parent), ui(new Ui::WifiDialog), networkManager(networkManager)
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

    networkManager->connectWifiAsync();
}

void WifiDialog::connectFinished(bool connected)
{
    // the result of an older attempt, e.g. one from before a suspend
    if (networkManager->isConnectingWifi())
        return;

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
