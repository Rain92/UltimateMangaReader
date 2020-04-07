#ifndef MENUEDIALOG_H
#define MENUEDIALOG_H

#include <QDialog>

#include "sizes.h"

enum MenueButton
{
    ExitButton = 3,
    SettingsButton,
    ClearDownloadsButton,
    UpdateMangaListsButton
};

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
    void on_pushButtonExit_clicked();
    void on_pushButtonSettings_clicked();
    void on_pushButtonClearDownloads_clicked();
    void on_pushButtonUpdateMangaList_clicked();

private:
    Ui::MenueDialog *ui;
};

#endif  // MENUEDIALOG_H
