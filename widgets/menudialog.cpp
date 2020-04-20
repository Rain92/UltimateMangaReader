#include "menudialog.h"

#include "ui_menudialog.h"

MenuDialog::MenuDialog(QWidget *parent) : QDialog(parent), ui(new Ui::MenuDialog)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Popup);

    ui->toolButtonMenu->setFixedSize(QSize(SIZES.menuIconSize, SIZES.menuIconSize));
    ui->toolButtonMenu->setIconSize(QSize(SIZES.menuIconSize, SIZES.menuIconSize));

    ui->pushButtonExit->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonSettings->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonClearDownloads->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonUpdateMangaList->setFixedHeight(SIZES.buttonSize);
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
