#include "updatemangalistsdialog.h"

#include "ui_updatemangalistsdialog.h"

UpdateMangaListsDialog::UpdateMangaListsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::UpdateMangaListsDialog)
{
    ui->setupUi(this);

    ui->pushButtonCancel->setFixedHeight(buttonsize);
    ui->pushButtonOk->setFixedHeight(buttonsize);
    ui->labelTitle->setStyleSheet("font-size: 15pt");
}

UpdateMangaListsDialog::~UpdateMangaListsDialog()
{
    delete ui;
}

void UpdateMangaListsDialog::setSettings(Settings *settings)
{
    this->settings = settings;
}

void UpdateMangaListsDialog::resetUI()
{
    setupSourcesList();
    ui->pushButtonOk->setText(" Ok ");
    ui->pushButtonOk->show();
    ui->frameMangaScources->show();
    ui->frameProgress->hide();
}

void UpdateMangaListsDialog::setupSourcesList()
{
    auto layout = ui->frameMangaScources->layout();

    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr)
    {
        if (item->widget())
            item->widget()->setParent(nullptr);
        delete item;
    }

    for (auto ms : settings->enabledMangaSources.keys())
    {
        QCheckBox *checkbox = new QCheckBox(ms, ui->frameMangaScources);
        checkbox->setChecked(true);
        layout->addWidget(checkbox);
    }
}

void UpdateMangaListsDialog::on_pushButtonOk_clicked() {}

void UpdateMangaListsDialog::on_pushButtonCancel_clicked()
{
    close();
}
