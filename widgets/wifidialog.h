#ifndef WIFIDIALOG_H
#define WIFIDIALOG_H

#include <QDialog>
#include <QtConcurrent/QtConcurrent>

#include "downloadmanager.h"

namespace Ui
{
class WifiDialog;
}

class WifiDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WifiDialog(QWidget* parent, DownloadManager* downloadManager);
    ~WifiDialog();

    void connect();

private slots:
    void on_pushButtonRetry_clicked();

    void on_pushButtonClose_clicked();

private:
    Ui::WifiDialog* ui;
    DownloadManager* downloadManager;
};

#endif  // WIFIDIALOG_H
