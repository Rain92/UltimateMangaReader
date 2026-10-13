#ifndef WIFIDIALOG_H
#define WIFIDIALOG_H

#include <QDialog>

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

    // Connects in the background, the dialog only shows up when that fails. With showProgress it shows up
    // right away (when the user asked for the connection).
    void connect(bool showProgress = false);
    // Closes the dialog and ignores the result of a running attempt (e.g. for a suspend).
    void cancel();

private slots:
    void on_pushButtonRetry_clicked();

    void on_pushButtonClose_clicked();

private:
    Ui::WifiDialog* ui;
    NetworkManager* networkManager;
    bool awaitingResult;

    void connectFinished(bool connected);
};

#endif  // WIFIDIALOG_H
