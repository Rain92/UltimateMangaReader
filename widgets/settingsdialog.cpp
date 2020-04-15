#include "settingsdialog.h"

#include "ui_settingsdialog.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::SettingsDialog), settings(nullptr)
{
    ui->setupUi(this);
    adjustSizes();
    setWindowFlags(Qt::Popup);

    QObject::connect(ui->checkBoxDoublePages, &QCheckBox::clicked, this, &SettingsDialog::updateSettings);

    QLayoutItem *item;
    int i = 0;
    while ((item = ui->verticalLayout->itemAt(i++)) != nullptr)
    {
        auto *checkbox = dynamic_cast<QCheckBox *>(item->widget());
        if (checkbox != nullptr)
        {
            QObject::connect(checkbox, &QCheckBox::clicked, this, &SettingsDialog::updateSettings);
        }
    }
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::setSettings(Settings *settings)
{
    this->settings = settings;
}

void SettingsDialog::adjustSizes()
{
    ui->labelTitle->setStyleSheet("font-size: 15pt");
    ui->pushButtonOk->setFixedHeight(buttonsize);
}

void SettingsDialog::resetUI()
{
    ui->checkBoxDoublePages->setChecked(settings->doublePageFullscreen);
    ui->checkBoxHideErrorMessages->setChecked(settings->hideErrorMessages);
    ui->checkBoxReverseSwipeDirection->setChecked(settings->reverseSwipeDirection);
    ui->checkBoxReverseButtonDirection->setChecked(settings->reverseButtonDirection);

    setupSourcesList();
}

void SettingsDialog::updateActiveMangasSettings(const QString &name, bool enabled)
{
    settings->enabledMangaSources[name] = enabled;
    settings->scheduleSerialize();

    emit activeMangasChanged();
}

void SettingsDialog::updateSettings()
{
    settings->doublePageFullscreen = ui->checkBoxDoublePages->isChecked();
    settings->hideErrorMessages = ui->checkBoxHideErrorMessages->isChecked();
    settings->reverseSwipeDirection = ui->checkBoxReverseSwipeDirection->isChecked();
    settings->reverseButtonDirection = ui->checkBoxReverseButtonDirection->isChecked();

    settings->scheduleSerialize();
}

void SettingsDialog::setupSourcesList()
{
    auto layout = ui->frameMangaScources->layout();

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

        QCheckBox *checkbox = new QCheckBox(ms, ui->frameMangaScources);
        checkbox->setChecked(enabled);
        QObject::connect(checkbox, &QCheckBox::clicked, this,
                         [this, checkbox, ms]() { updateActiveMangasSettings(ms, checkbox->isChecked()); });

        layout->addWidget(checkbox);
    }
}

void SettingsDialog::on_pushButtonOk_clicked()
{
    close();
}
