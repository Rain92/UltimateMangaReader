#include "menudialog.h"

#include "ui_menudialog.h"

MenuDialog::MenuDialog(QWidget *parent) : QDialog(parent), ui(new Ui::MenuDialog)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Popup);

    ui->toolButtonMenu->setFixedSize(QSize(menuiconsize, menuiconsize));
    ui->toolButtonMenu->setIconSize(QSize(menuiconsize, menuiconsize));

    ui->pushButtonExit->setFixedHeight(buttonsize);
    ui->pushButtonSettings->setFixedHeight(buttonsize);
    ui->pushButtonClearDownloads->setFixedHeight(buttonsize);
    ui->pushButtonUpdateMangaList->setFixedHeight(buttonsize);
}

MenuDialog::~MenuDialog()
{
    delete ui;
}

void MenuDialog::on_toolButtonMenu_clicked()
{
    close();
}

void MenuDialog::on_pushButtonExit_clicked()
{
    done(ExitButton);
}

void MenuDialog::on_pushButtonSettings_clicked()
{
    done(SettingsButton);
}

void MenuDialog::on_pushButtonClearDownloads_clicked()
{
    done(ClearDownloadsButton);
}
void MenuDialog::on_pushButtonUpdateMangaList_clicked()
{
    done(UpdateMangaListsButton);
}
