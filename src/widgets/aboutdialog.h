#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

#include "aboutinfo.h"
#include "sizes.h"

namespace Ui
{
class AboutDialog;
}

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent);
    ~AboutDialog();

private slots:
    void on_pushButtonClose_clicked();

private:
    Ui::AboutDialog *ui;
};

#endif  // ABOUTDIALOG_H
