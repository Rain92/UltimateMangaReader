#ifndef MENUDIALOG_H
#define MENUDIALOG_H

#include <QDialog>

#include "sizes.h"

enum MenuButton
{
    ExitButton = 3,
    SettingsButton,
    ClearDownloadsButton,
    AboutButton,
    UpdateMangaListsButton
};

namespace Ui
{
class MenuDialog;
}

class MenuDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MenuDialog(QWidget *parent = nullptr);
    ~MenuDialog();

private slots:
    void on_toolButtonMenu_clicked();
    void on_pushButtonExit_clicked();
    void on_pushButtonSettings_clicked();
    void on_pushButtonClearDownloads_clicked();
    void on_pushButtonUpdateMangaList_clicked();

    void on_pushButtonAbout_clicked();

private:
    Ui::MenuDialog *ui;
};

#endif  // MENUDIALOG_H
