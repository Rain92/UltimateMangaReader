#include "gotodialog.h"
#include "ui_gotodialog.h"
#include "configs.h"
#include "numpadwidget.h"

GotoDialog::GotoDialog(QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog),
    selectedindex(-1, -1, true),
    currentindex(-1, -1, true),
    ui(new Ui::GotoDialog)

{
    ui->setupUi(this);
    QString ss = "QDialog{                  "
                 "border: 2px solid black;  "
                 "background: white;        "
                 "}                         ";
    setWindowModality(Qt::WindowModal);
    setStyleSheet(ss);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    ui->pushButtonGoChapter->setMinimumHeight(buttonsize);
    ui->pushButtonGoPage->setMinimumHeight(buttonsize);
    ui->spinBoxChapter->setMinimumHeight(buttonsize);
    ui->spinBoxPage->setMinimumHeight(buttonsize);

    ui->buttonBox->buttons()[0]->setMinimumHeight(buttonsize);

    ui->spinBoxChapter->installEventFilter(this);
    ui->spinBoxPage->installEventFilter(this);

//    this->setMinimumSize(QSize(mm_to_px(50), mm_to_px(40)));

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
    ui->spinBoxPage->setValue(currentindex. page + 1);


    ui->spinBoxChapter->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);

    this->setMinimumSize(this->parentWidget()->width() * 0.8, this->parentWidget()->height() * 0.6);

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
            if ( (key->key() == Qt::Key_Enter) || (key->key() == Qt::Key_Return) )
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
        QKeyEvent *pressEvent = new QKeyEvent(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QKeyEvent *releaseEvent = new QKeyEvent(QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier);

        qApp->postEvent(target, pressEvent);
        qApp->postEvent(target, releaseEvent);
    }
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
