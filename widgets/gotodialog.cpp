#include "gotodialog.h"
#include "ui_gotodialog.h"

GotoDialog::GotoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GotoDialog),
    selectedindex(-1, -1, true),
    currentindex(-1, -1, true)

{
    ui->setupUi(this);
}

GotoDialog::~GotoDialog()
{
    delete ui;
}

void GotoDialog::setup(int maxchapter, int maxpage, MangaIndex currentindex)
{
    this->currentindex = currentindex;
    this->selectedindex = MangaIndex(-1, -1, true);

    ui->spinBoxChapter->setMaximum(maxchapter);
    ui->spinBoxChapter->setValue(currentindex.chapter + 1);

    ui->spinBoxPage->setMaximum(maxpage);
    ui->spinBoxPage->setValue(currentindex.page + 1);

}

void GotoDialog::on_pushButtonGoChapter_clicked()
{
    this->selectedindex = MangaIndex(ui->spinBoxChapter->value() - 1, 0);
    this->accept();
}

void GotoDialog::on_pushButtonGoPage_clicked()
{
    this->selectedindex = MangaIndex(currentindex.chapter, ui->spinBoxPage->value() - 1);
    this->accept();
}
