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
    ui->pushButtonConfirm->setFixedHeight(buttonsize);
    ui->pushButtonCancel->setFixedHeight(buttonsize);
}

DownloadMangaChaptersDialog::~DownloadMangaChaptersDialog()
{
    delete ui;
}

void DownloadMangaChaptersDialog::show(QSharedPointer<MangaInfo> mangaInfo, int chapterFrom)
{
    this->setMaximumWidth(static_cast<QWidget *>(this->parent())->width());

    this->mangaInfo = mangaInfo;
    ui->spinBoxFrom->setRange(chapterFrom + 1, mangaInfo->chapters.size());
    ui->spinBoxTo->setRange(1, mangaInfo->chapters.size());
    ui->spinBoxFrom->setValue(1);
    ui->spinBoxTo->setValue(mangaInfo->chapters.size());

    ui->labelTitle->setText("Download " + mangaInfo->title);

    open();
    ui->spinBoxTo->setFocus();
}

void DownloadMangaChaptersDialog::on_pushButtonCancel_clicked()
{
    qDebug() << "cancel clicked";
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
            on_pushButtonConfirm_clicked();
        }
    }

    return QObject::eventFilter(obj, event);
}

void DownloadMangaChaptersDialog::on_pushButtonConfirm_clicked()
{
    int from = ui->spinBoxFrom->value() - 1;
    int to = ui->spinBoxTo->value() - 1;

    if (from <= to)
    {
        close();
        emit downloadConfirmed(mangaInfo, from, to);
        mangaInfo.clear();
    }
}
