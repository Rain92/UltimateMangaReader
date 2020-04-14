#include "settingswidget.h"

#include "ui_settingswidget.h"

SettingsWidget::SettingsWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::SettingsWidget)
{
    ui->setupUi(this);

    QObject::connect(ui->checkBoxDoublePages, &QCheckBox::clicked, this,
                     &SettingsWidget::updateSettings);

    QLayoutItem *item;
    int i = 0;
    while ((item = ui->verticalLayout->itemAt(i++)) != nullptr)
    {
        auto *checkbox = dynamic_cast<QCheckBox *>(item->widget());
        if (checkbox != nullptr)
            QObject::connect(checkbox, &QCheckBox::clicked, this,
                             &SettingsWidget::updateSettings);
    }
}

SettingsWidget::~SettingsWidget() { delete ui; }

void SettingsWidget::setSettings(Settings *settings)
{
    this->settings = settings;
}

void SettingsWidget::updateUI()
{
    ui->checkBoxDoublePages->setChecked(settings->doublePageFullscreen);
    ui->checkBoxHideErrorMessages->setChecked(settings->hideErrorMessages);
    ui->checkBoxReverseSwipeDirection->setChecked(
        settings->reverseSwipeDirection);
    ui->checkBoxReverseButtonDirection->setChecked(
        settings->reverseButtonDirection);

    setupSourcesList();
}

void SettingsWidget::updateActiveMangasSettings(const QString &name,
                                                bool enabled)
{
    settings->enabledMangaSources[name] = enabled;
    settings->scheduleSerialize();

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

    settings->scheduleSerialize();
}

void SettingsWidget::setupSourcesList()
{
    auto layout = ui->frameEnabledSources->layout();

    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr)
    {
        if (item->widget())
            item->widget()->setParent(nullptr);
        delete item;
    }

    for (auto ms : settings->enabledMangaSources.keys())
    {
        bool enabled = settings->enabledMangaSources[ms];

        QCheckBox *checkbox = new QCheckBox(ms, ui->frameEnabledSources);
        checkbox->setChecked(enabled);
        QObject::connect(
            checkbox, &QCheckBox::clicked, this, [this, checkbox, ms]() {
                updateActiveMangasSettings(ms, checkbox->isChecked());
            });

        layout->addWidget(checkbox);
    }
}
