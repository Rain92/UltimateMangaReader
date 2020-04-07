#include "menuedialog.h"

#include "ui_menuedialog.h"

MenueDialog::MenueDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::MenueDialog)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Popup);

    ui->toolButtonMenu->setFixedSize(QSize(menueiconsize, menueiconsize));
    ui->pushButtonExit->setFixedHeight(buttonsize);
    ui->pushButtonSettings->setFixedHeight(buttonsize);
    ui->pushButtonClearDownloads->setFixedHeight(buttonsize);
    ui->pushButtonUpdateMangaList->setFixedHeight(buttonsize);
}

MenueDialog::~MenueDialog() { delete ui; }

void MenueDialog::on_toolButtonMenu_clicked() { close(); }

void MenueDialog::on_pushButtonExit_clicked() { done(ExitButton); }

void MenueDialog::on_pushButtonSettings_clicked() { done(SettingsButton); }

void MenueDialog::on_pushButtonClearDownloads_clicked()
{
    done(ClearDownloadsButton);
}
void MenueDialog::on_pushButtonUpdateMangaList_clicked()
{
    done(UpdateMangaListsButton);
}
