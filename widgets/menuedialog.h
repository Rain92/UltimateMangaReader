#ifndef MENUEDIALOG_H
#define MENUEDIALOG_H

#include <QDialog>

#include "sizes.h"

namespace Ui
{
class MenueDialog;
}

class MenueDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MenueDialog(QWidget *parent = nullptr);
    ~MenueDialog();

private slots:
    void on_toolButtonMenu_clicked();

private:
    Ui::MenueDialog *ui;
};

#endif  // MENUEDIALOG_H
