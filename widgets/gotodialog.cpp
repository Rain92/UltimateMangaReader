#include "gotodialog.h"

#include "ui_gotodialog.h"

GotoDialog::GotoDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog), selectedindex(0, 0), currentindex(0, 0), ui(new Ui::GotoDialog)

{
    ui->setupUi(this);

    ui->pushButtonGoChapter->setFixedHeight(buttonsize);
    ui->pushButtonGoPage->setFixedHeight(buttonsize);
    ui->spinBoxChapter->setFixedHeight(buttonsize);
    ui->spinBoxPage->setFixedHeight(buttonsize);

    ui->buttonBox->buttons()[0]->setFixedHeight(buttonsize);

    ui->spinBoxChapter->installEventFilter(this);
    ui->spinBoxPage->installEventFilter(this);

    //    this->setMinimumSize(QSize(mm_to_px(50), mm_to_px(40)));
}

GotoDialog::~GotoDialog()
{
    delete ui;
}

void GotoDialog::setup(const ReadingProgress &progress)
{
    this->currentindex = MangaIndex(progress.index);
    this->selectedindex = MangaIndex(progress.index);

    ui->spinBoxChapter->setMaximum(progress.numChapters);
    ui->spinBoxChapter->setValue(currentindex.chapter + 1);

    ui->spinBoxPage->setMaximum(progress.numPages);
    ui->spinBoxPage->setValue(currentindex.page + 1);

    ui->spinBoxChapter->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);

    this->setMinimumSize(this->parentWidget()->width() * 0.8, this->parentWidget()->height() * 0.6);
}

bool GotoDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        static_cast<QSpinBox *>(obj)->selectAll();
        return false;
    }
    else if (event->type() == QEvent::RequestSoftwareInputPanel)
    {
        return true;
    }
    else if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *key = static_cast<QKeyEvent *>(event);
        if ((key->key() == Qt::Key_Enter) || (key->key() == Qt::Key_Return))
        {
            if (obj == ui->spinBoxChapter)
                on_pushButtonGoChapter_clicked();
            if (obj == ui->spinBoxPage)
                on_pushButtonGoPage_clicked();

            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}

void GotoDialog::on_pushButtonGoChapter_clicked()
{
    this->selectedindex.chapter = ui->spinBoxChapter->value() - 1;
    this->selectedindex.page = 0;
    this->accept();
}

void GotoDialog::on_pushButtonGoPage_clicked()
{
    this->selectedindex.page = ui->spinBoxPage->value() - 1;
    this->accept();
}
