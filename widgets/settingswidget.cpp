#include "settingswidget.h"

#include "ui_settingswidget.h"

SettingsWidget::SettingsWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::SettingsWidget)
{
    ui->setupUi(this);
}

SettingsWidget::~SettingsWidget() { delete ui; }

void SettingsWidget::setSettings(Settings *settings)
{
    this->settings = settings;
    setupSourcesList();
    updateUI();
}

void SettingsWidget::updateUI()
{
    ui->checkBoxDoublePages->setChecked(settings->doublePageFullscreen);
    ui->checkBoxHideErrorMessages->setChecked(settings->hideErrorMessages);
    ui->checkBoxReverseSwipeDirection->setChecked(
        settings->reverseSwipeDirection);
    ui->checkBoxReverseButtonDirection->setChecked(
        settings->reverseButtonDirection);
}

void SettingsWidget::updateActiveMangasSettings()
{
    settings->mangaSourcesEnabled.clear();

    for (int i = 0; i < settings->mangaSourcesEnabled.count(); i++)
    {
        auto name = ui->tableWidgetMangaSourcesEnabled->item(i, 0)->text();
        auto enabled = static_cast<QCheckBox *>(
                           ui->tableWidgetMangaSourcesEnabled->cellWidget(i, 0))
                           ->isChecked();

        settings->mangaSourcesEnabled.append({name, enabled});
    }

    emit activeMangasChanged();
}

void SettingsWidget::updateSettings()
{
    settings->doublePageFullscreen = ui->checkBoxDoublePages->isChecked();
    settings->hideErrorMessages = ui->checkBoxHideErrorMessages->isChecked();
    settings->reverseSwipeDirection =
        ui->checkBoxReverseSwipeDirection->isChecked();
    settings->reverseButtonDirection =
        ui->checkBoxReverseButtonDirection->isChecked();
}

void SettingsWidget::setupSourcesList()
{
    ui->tableWidgetMangaSourcesEnabled->clear();

    for (int i = 0; i < settings->mangaSourcesEnabled.count(); i++)
    {
        ui->tableWidgetMangaSourcesEnabled->insertRow(i);

        QTableWidgetItem *nameItem =
            new QTableWidgetItem(settings->mangaSourcesEnabled[i].first);

        bool enabled = settings->mangaSourcesEnabled[i].second;

        QCheckBox *enabledCheckbox =
            new QCheckBox(ui->tableWidgetMangaSourcesEnabled);
        enabledCheckbox->setChecked(enabled);
        QObject::connect(enabledCheckbox, &QCheckBox::clicked, this,
                         &SettingsWidget::updateActiveMangasSettings);

        ui->tableWidgetMangaSourcesEnabled->setItem(i, 0, nameItem);
        ui->tableWidgetMangaSourcesEnabled->setCellWidget(i, 1,
                                                          enabledCheckbox);
    }
}
