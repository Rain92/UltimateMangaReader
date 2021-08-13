#ifndef WIFIDIALOG_H
#define WIFIDIALOG_H

#include <QDialog>
#include <QtConcurrent/QtConcurrent>

#include "networkmanager.h"

namespace Ui
{
class WifiDialog;
}

class WifiDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WifiDialog(QWidget* parent, NetworkManager* networkManager);
    ~WifiDialog();

    void connect();

private slots:
    void on_pushButtonRetry_clicked();

    void on_pushButtonClose_clicked();

private:
    Ui::WifiDialog* ui;
    NetworkManager* networkManager;
    QFuture<void> lastConnection;
};

#endif  // WIFIDIALOG_H
