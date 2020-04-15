#ifndef UPDATEMANGALISTSDIALOG_H
#define UPDATEMANGALISTSDIALOG_H

#include <QDialog>

#include "settings.h"
#include "sizes.h"

namespace Ui
{
class UpdateMangaListsDialog;
}

class UpdateMangaListsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateMangaListsDialog(QWidget *parent = nullptr);
    ~UpdateMangaListsDialog();

    void setSettings(Settings *settings);
    void resetUI();

private slots:
    void on_pushButtonOk_clicked();

    void on_pushButtonCancel_clicked();

private:
    Ui::UpdateMangaListsDialog *ui;
    Settings *settings;

    void setupSourcesList();
};

#endif  // UPDATEMANGALISTSDIALOG_H
