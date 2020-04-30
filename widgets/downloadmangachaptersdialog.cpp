#include "downloadmangachaptersdialog.h"

#include "ui_downloadmangachaptersdialog.h"

DownloadMangaChaptersDialog::DownloadMangaChaptersDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::DownloadMangaChaptersDialog)
{
    ui->setupUi(this);
    adjustUI();

    ui->spinBoxFrom->installEventFilter(this);
    ui->spinBoxTo->installEventFilter(this);
}

void DownloadMangaChaptersDialog::adjustUI()
{
    ui->labelTitle->setStyleSheet("font-size: 12pt");
    ui->pushButtonConfirm->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonCancel->setFixedHeight(SIZES.buttonSize);
    ui->spinBoxFrom->setFixedHeight(SIZES.buttonSize);
    ui->spinBoxTo->setFixedHeight(SIZES.buttonSize);
    ui->numpadWidget->setMinimumHeight(SIZES.numpadHeight);
}

DownloadMangaChaptersDialog::~DownloadMangaChaptersDialog()
{
    delete ui;
}

void DownloadMangaChaptersDialog::show(QSharedPointer<MangaInfo> mangaInfo, int chapterFromDefault)
{
    this->setMaximumWidth(static_cast<QWidget *>(this->parent())->width());

    this->mangaInfo = mangaInfo;
    ui->spinBoxFrom->setRange(1, mangaInfo->chapters.size());
    ui->spinBoxTo->setRange(1, mangaInfo->chapters.size());
    ui->spinBoxFrom->setValue(chapterFromDefault + 1);
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
