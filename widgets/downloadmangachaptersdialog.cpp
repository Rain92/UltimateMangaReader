#include "downloadmangachaptersdialog.h"

#include "ui_downloadmangachaptersdialog.h"

DownloadMangaChaptersDialog::DownloadMangaChaptersDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::DownloadMangaChaptersDialog)
{
    ui->setupUi(this);
    adjustSizes();

    ui->spinBoxFrom->installEventFilter(this);
    ui->spinBoxTo->installEventFilter(this);
}

void DownloadMangaChaptersDialog::adjustSizes()
{
    ui->labelTitle->setStyleSheet("font-size: 12pt");
    ui->pushButtonOk->setFixedHeight(buttonsize);
    ui->pushButtonCancel->setFixedHeight(buttonsize);
}

DownloadMangaChaptersDialog::~DownloadMangaChaptersDialog()
{
    delete ui;
}

void DownloadMangaChaptersDialog::show(QSharedPointer<MangaInfo> mangaInfo)
{
    this->setMaximumWidth(static_cast<QWidget *>(this->parent())->width());

    this->mangaInfo = mangaInfo;
    ui->spinBoxFrom->setRange(1, mangaInfo->chapters.size());
    ui->spinBoxTo->setRange(1, mangaInfo->chapters.size());
    ui->spinBoxFrom->setValue(1);
    ui->spinBoxTo->setValue(mangaInfo->chapters.size());

    ui->labelTitle->setText("Download " + mangaInfo->title);

    open();
    ui->spinBoxTo->setFocus();
}

void DownloadMangaChaptersDialog::on_pushButtonOk_clicked()
{
    if (ui->spinBoxFrom->value() <= ui->spinBoxTo->value())
    {
        emit downloadConfirmed(mangaInfo, ui->spinBoxFrom->value(), ui->spinBoxTo->value());
        mangaInfo.clear();
        close();
    }
}

void DownloadMangaChaptersDialog::on_pushButtonCancel_clicked()
{
    mangaInfo.clear();
    close();
}

bool DownloadMangaChaptersDialog::eventFilter(QObject *obj, QEvent *event)
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
            on_pushButtonOk_clicked();
        }
    }

    return QObject::eventFilter(obj, event);
}
