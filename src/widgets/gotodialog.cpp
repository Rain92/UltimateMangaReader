#include "gotodialog.h"

#include "ui_gotodialog.h"

GotoDialog::GotoDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog), selectedindex(0, 0), currentindex(0, 0), ui(new Ui::GotoDialog)

{
    ui->setupUi(this);
    setWindowFlags(Qt::Popup);

    ui->pushButtonGoChapter->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonGoPage->setFixedHeight(SIZES.buttonSize);
    ui->spinBoxChapter->setFixedHeight(SIZES.buttonSize);
    ui->spinBoxPage->setFixedHeight(SIZES.buttonSize);

    ui->pushButtonCancel->setFixedHeight(SIZES.buttonSize);

    ui->spinBoxChapter->installEventFilter(this);
    ui->spinBoxPage->installEventFilter(this);

    ui->numpadWidget->setFixedHeight(SIZES.numpadHeight);
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
}

bool GotoDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        auto box = static_cast<QSpinBox *>(obj);
        QTimer::singleShot(0, box, &QSpinBox::selectAll);

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

void GotoDialog::on_pushButtonCancel_clicked()
{
    close();
}
