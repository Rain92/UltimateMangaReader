#include "updatemangalistsdialog.h"

#include "sourcechecklist.h"
#include "ui_updatemangalistsdialog.h"

UpdateMangaListsDialog::UpdateMangaListsDialog(SourceRegistry *sources, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::UpdateMangaListsDialog),
      sources(new SourceListModel(sources, SourceListModel::SelectSources, this)),
      progressToken(nullptr),
      updateCompleted(false)
{
    ui->setupUi(this);

    ui->pushButtonCancel->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonOk->setFixedHeight(SIZES.buttonSize);
    ui->labelTitle->setStyleSheet("font-size: 15pt");

    auto sourceList = new SourceCheckList(this->sources, ui->frameMangaSources);
    sourceList->layout()->setSpacing(ui->frameMangaSources->layout()->spacing());
    ui->frameMangaSources->layout()->addWidget(sourceList);
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
    // all of them, but the ones an unfinished update completed (below)
    sources->selectAll();
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
        if (!updateCompleted)
            for (auto [name, progress] : progressToken->sourcesProgress.asKeyValueRange())
                if (progress == 100)
                    sources->setChecked(name, false);

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
    // a retry continues where the sources stopped (the token keeps what they got done)
    for (const auto &name : sources->checkedNames())
        if (!progressToken->sourcesProgress.contains(name))
            progressToken->sourcesProgress.insert(name, 0);

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
