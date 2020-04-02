#include "gotodialog.h"

#include "ui_gotodialog.h"

GotoDialog::GotoDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog),
      selectedindex(0, 0),
      currentindex(0, 0),
      ui(new Ui::GotoDialog)

{
    ui->setupUi(this);
    setWindowModality(Qt::WindowModal);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    ui->pushButtonGoChapter->setFixedHeight(buttonsize);
    ui->pushButtonGoPage->setFixedHeight(buttonsize);
    ui->spinBoxChapter->setFixedHeight(buttonsize);
    ui->spinBoxPage->setFixedHeight(buttonsize);

    ui->buttonBox->buttons()[0]->setFixedHeight(buttonsize);

    ui->spinBoxChapter->installEventFilter(this);
    ui->spinBoxPage->installEventFilter(this);

    //    this->setMinimumSize(QSize(mm_to_px(50), mm_to_px(40)));
}

GotoDialog::~GotoDialog() { delete ui; }

void GotoDialog::setup(const ReadingProgress &progress)
{
    this->currentindex = MangaIndex(progress.index);
    this->selectedindex = MangaIndex(progress.index);

    ui->spinBoxChapter->setMaximum(progress.numChapters);
    ui->spinBoxChapter->setValue(currentindex.chapter + 1);

    ui->spinBoxPage->setMaximum(progress.numPages);
    ui->spinBoxPage->setValue(currentindex.page + 1);

    ui->spinBoxChapter->setCorrectionMode(
        QAbstractSpinBox::CorrectToNearestValue);

    this->setMinimumSize(this->parentWidget()->width() * 0.8,
                         this->parentWidget()->height() * 0.6);
}

bool GotoDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->spinBoxChapter || obj == ui->spinBoxPage)
    {
        if (event->type() == QEvent::FocusIn)
        {
            moveCursorRight(4);
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
            else
            {
                return QObject::eventFilter(obj, event);
            }
        }
        else
        {
            return false;
        }
    }
    else
    {
        return QWidget::eventFilter(obj, event);
    }
}

void GotoDialog::moveCursorRight(int steps)
{
    QWidget *target = qApp->focusWidget();

    for (int i = 0; i < steps; i++)
    {
        QKeyEvent *pressEvent =
            new QKeyEvent(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QKeyEvent *releaseEvent =
            new QKeyEvent(QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier);

        qApp->postEvent(target, pressEvent);
        qApp->postEvent(target, releaseEvent);
    }
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
