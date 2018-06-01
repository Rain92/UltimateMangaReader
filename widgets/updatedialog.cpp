#include "updatedialog.h"
#include "ui_updatedialog.h"
#include "configs.h"

UpdateDialog::UpdateDialog(QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog),
    ui(new Ui::UpdateDialog)
{
    ui->setupUi(this);


    QString ss = "QDialog{                  "
                 "border: 2px solid black;  "
                 "background: white;        "
                 "}                         ";
    setWindowModality(Qt::WindowModal);
    setStyleSheet(ss);

    ui->pushButtonOk->setMinimumHeight(buttonsize);
    ui->pushButtonRetry->setMinimumHeight(buttonsize);
    ui->progressBar->setMinimumHeight(buttonsize);
}

UpdateDialog::~UpdateDialog()
{
    delete ui;
}


void UpdateDialog::setup(int maximum, const QString &msg)
{
    ui->progressBar->setMaximum(maximum);
    ui->progressBar->setValue(0);
    ui->labelStatus->setText(msg);

    ui->pushButtonOk->hide();
    ui->pushButtonRetry->hide();
}

void UpdateDialog::error(const QString &msg)
{
    ui->labelStatus->setText(msg);

    ui->pushButtonOk->setText("Nevermind");
    ui->pushButtonOk->show();
    ui->pushButtonRetry->show();
}

void UpdateDialog::setLabelText(const QString &msg)
{
    ui->labelStatus->setText(msg);
}

void UpdateDialog::updateProgress(int i)
{
    ui->progressBar->setValue(i);
    if(i==ui->progressBar->maximum())
    {
        ui->labelStatus->setText("Update completed!");
        ui->pushButtonOk->setText("Sweet");
        ui->pushButtonOk->show();
    }
}


void UpdateDialog::on_pushButtonRetry_clicked()
{
    emit retry();
}

void UpdateDialog::on_pushButtonOk_clicked()
{
    this->close();
}
