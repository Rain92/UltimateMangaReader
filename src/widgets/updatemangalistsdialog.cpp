#include "updatemangalistsdialog.h"

#include "ui_updatemangalistsdialog.h"

UpdateMangaListsDialog::UpdateMangaListsDialog(Settings *settings, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::UpdateMangaListsDialog),
      settings(settings),
      progressToken(nullptr),
      updateCompleted(false)
{
    ui->setupUi(this);

    ui->pushButtonCancel->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonOk->setFixedHeight(SIZES.buttonSize);
    ui->labelTitle->setStyleSheet("font-size: 15pt");
}

UpdateMangaListsDialog::~UpdateMangaListsDialog()
{
    delete ui;
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
    ui->frameMangaSources->show();
    ui->frameProgress->hide();

    ui->labelProgress->setText("Starting...");
    ui->progressBar->setValue(0);

    if (progressToken)
    {
        QLayoutItem *item;
        int i = 0;
        while ((item = ui->frameMangaSources->layout()->itemAt(i++)) != nullptr)
        {
            auto *checkbox = dynamic_cast<QCheckBox *>(item->widget());
            if (checkbox != nullptr && !updateCompleted)
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

    updateCompleted = false;
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
    auto layout = ui->frameMangaSources->layout();

    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr)
    {
        delete item->widget();
        delete item;
    }

    for (const auto &ms : settings->enabledMangaSources.keys())
    {
        if (!settings->enabledMangaSources[ms])
            continue;

        QCheckBox *checkbox = new QCheckBox(ms, ui->frameMangaSources);
        checkbox->setChecked(true);
        layout->addWidget(checkbox);
    }
}

void UpdateMangaListsDialog::updateProgress()
{
    int numsources = progressToken->sourcesProgress.count();
    int sum = 0;
    for (auto v : progressToken->sourcesProgress.values())
        sum += v;
    ui->labelProgress->setText("Updating " + progressToken->currentSourceName + "...");
    ui->progressBar->setValue(sum / numsources);
}

void UpdateMangaListsDialog::updateFinished(const QString &message)
{
    updateCompleted = true;
    ui->labelProgress->setText(message.isEmpty() ? QString("All updates completed!")
                                                 : "All updates completed. " + message);

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
    while ((item = ui->frameMangaSources->layout()->itemAt(i++)) != nullptr)
    {
        auto *checkbox = dynamic_cast<QCheckBox *>(item->widget());
        // a retry continues where the sources stopped (the token keeps what they got done)
        if (checkbox != nullptr && checkbox->isChecked() &&
            !progressToken->sourcesProgress.contains(checkbox->text()))
            progressToken->sourcesProgress.insert(checkbox->text(), 0);
    }

    if (progressToken->sourcesProgress.count() == 0)
        return;

    ui->frameMangaSources->hide();
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
