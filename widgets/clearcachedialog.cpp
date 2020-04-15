#include "clearcachedialog.h"

#include "ui_clearcachedialog.h"

ClearCacheDialog::ClearCacheDialog(QWidget* parent) : QDialog(parent), ui(new Ui::ClearCacheDialog)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Popup);

    ui->pushButtonCancel->setFixedHeight(buttonsize);
    ui->pushButtonClear1->setFixedHeight(buttonsize);
    ui->pushButtonClear2->setFixedHeight(buttonsize);
    ui->pushButtonClear3->setFixedHeight(buttonsize);
}

ClearCacheDialog::~ClearCacheDialog()
{
    delete ui;
}

void ClearCacheDialog::setValues(long chacheSize, long freeSpace)
{
    QString str =
        QString("Downloads take up %1 MB. \n%2 MB of free space remaining.").arg(chacheSize).arg(freeSpace);
    ui->labelCacheSize->setText(str);
}

void ClearCacheDialog::on_pushButtonCancel_clicked()
{
    close();
}

void ClearCacheDialog::on_pushButtonClear1_clicked()
{
    finished(ClearImages);
    close();
}

void ClearCacheDialog::on_pushButtonClear2_clicked()
{
    finished(ClearInfos);
    close();
}

void ClearCacheDialog::on_pushButtonClear3_clicked()
{
    finished(ClearAll);
    close();
}
