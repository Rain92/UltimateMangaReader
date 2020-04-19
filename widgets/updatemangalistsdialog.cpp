#include "updatemangalistsdialog.h"

#include "ui_updatemangalistsdialog.h"

UpdateMangaListsDialog::UpdateMangaListsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::UpdateMangaListsDialog), progressToken(nullptr)
{
    ui->setupUi(this);

    ui->pushButtonCancel->setFixedHeight(buttonsize);
    ui->pushButtonOk->setFixedHeight(buttonsize);
    ui->labelTitle->setStyleSheet("font-size: 15pt");
}

UpdateMangaListsDialog::~UpdateMangaListsDialog()
{
    delete ui;
}

void UpdateMangaListsDialog::setSettings(Settings *settings)
{
    this->settings = settings;
}

void UpdateMangaListsDialog::open()
{
    resetUI();
    QDialog::open();
}

void UpdateMangaListsDialog::resetUI()
{
    setupSourcesList();
    ui->pushButtonOk->setText(" Ok ");
    ui->pushButtonOk->show();
    ui->pushButtonCancel->show();
    ui->pushButtonFinish->hide();
    ui->frameMangaScources->show();
    ui->frameProgress->hide();

    ui->labelProgress->setText("Starting...");
    ui->progressBar->setValue(0);

    if (progressToken)
    {
        QLayoutItem *item;
        int i = 0;
        while ((item = ui->frameMangaScources->layout()->itemAt(i++)) != nullptr)
        {
            auto *checkbox = dynamic_cast<QCheckBox *>(item->widget());
            if (checkbox != nullptr)
            {
                if ((progressToken->sourcesProgress.contains(checkbox->text()) &&
                     progressToken->sourcesProgress[checkbox->text()] == 100))
                    checkbox->setChecked(false);
            }
        }

        disconnect(progressToken.get(), &UpdateProgressToken::updateError, this,
                   &UpdateMangaListsDialog::updateError);
        disconnect(progressToken.get(), &UpdateProgressToken::updateFinished, this,
                   &UpdateMangaListsDialog::updateFinished);
        disconnect(progressToken.get(), &UpdateProgressToken::updateProgress, this,
                   &UpdateMangaListsDialog::updateProgress);
    }

    progressToken.reset(new UpdateProgressToken());
    connect(progressToken.get(), &UpdateProgressToken::updateError, this,
            &UpdateMangaListsDialog::updateError);
    connect(progressToken.get(), &UpdateProgressToken::updateFinished, this,
            &UpdateMangaListsDialog::updateFinished);
    connect(progressToken.get(), &UpdateProgressToken::updateProgress, this,
            &UpdateMangaListsDialog::updateProgress);
}

void UpdateMangaListsDialog::setupSourcesList()
{
    auto layout = ui->frameMangaScources->layout();

    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr)
    {
        delete item->widget();
        delete item;
    }

    for (auto ms : settings->enabledMangaSources.keys())
    {
        QCheckBox *checkbox = new QCheckBox(ms, ui->frameMangaScources);
        checkbox->setChecked(true);
        layout->addWidget(checkbox);
    }
}

void UpdateMangaListsDialog::updateProgress()
{
    int numsources = progressToken->sourcesProgress.values().count();
    int sum = 0;
    for (auto v : progressToken->sourcesProgress.values())
        sum += v;
    ui->labelProgress->setText("Updating " + progressToken->currentSourceName + "...");
    ui->progressBar->setValue(sum / numsources);
}

void UpdateMangaListsDialog::updateFinished()
{
    ui->labelProgress->setText("All updates completed!");
    ui->progressBar->setValue(100);
    ui->pushButtonOk->hide();
    ui->pushButtonCancel->hide();
    ui->pushButtonFinish->show();
}

void UpdateMangaListsDialog::updateError(const QString &message)
{
    ui->labelProgress->setText(message);
    ui->pushButtonOk->setText("Retry");
    ui->pushButtonOk->show();
}

void UpdateMangaListsDialog::on_pushButtonOk_clicked()
{
    QLayoutItem *item;
    int i = 0;
    while ((item = ui->frameMangaScources->layout()->itemAt(i++)) != nullptr)
    {
        auto *checkbox = dynamic_cast<QCheckBox *>(item->widget());
        if (checkbox != nullptr)
        {
            if (checkbox->isChecked() && !(progressToken->sourcesProgress.contains(checkbox->text()) &&
                                           progressToken->sourcesProgress[checkbox->text()] == 100))
                progressToken->sourcesProgress.insert(checkbox->text(), 0);
        }
    }

    if (progressToken->sourcesProgress.count() == 0)
        return;

    ui->frameMangaScources->hide();
    ui->pushButtonOk->hide();
    ui->frameProgress->show();
    emit updateClicked(progressToken);
}

void UpdateMangaListsDialog::on_pushButtonCancel_clicked()
{
    progressToken->canceled = true;
    close();
}

void UpdateMangaListsDialog::on_pushButtonFinish_clicked()
{
    close();
}
