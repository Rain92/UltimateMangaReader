#include "aboutdialog.h"

#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent), ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Popup);

    ui->pushButtonClose->setFixedHeight(SIZES.buttonSize);
    ui->labelText->setText(aboutString);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::on_pushButtonClose_clicked()
{
    close();
}
