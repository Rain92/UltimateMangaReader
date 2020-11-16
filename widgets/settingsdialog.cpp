#include "settingsdialog.h"

#include "ui_settingsdialog.h"

SettingsDialog::SettingsDialog(Settings *settings, QWidget *parent)
    : QDialog(parent), ui(new Ui::SettingsDialog), settings(settings), internalChange(false)
{
    ui->setupUi(this);
    adjustUI();
    setWindowFlags(Qt::Popup);

    for (auto item : this->findChildren<QComboBox *>())
        QObject::connect(item, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                         [this](int) { this->updateSettings(); });

    for (auto item : this->findChildren<QCheckBox *>())
        QObject::connect(item, &QCheckBox::clicked, this, &SettingsDialog::updateSettings);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::adjustUI()
{
    ui->labelTitle->setStyleSheet("font-size: 15pt");
    ui->pushButtonOk->setFixedHeight(SIZES.buttonSize);
}

void SettingsDialog::open()
{
    resetUI();
    QDialog::open();
}

void SettingsDialog::resetUI()
{
    internalChange = true;
    ui->checkBoxHideErrorMessages->setChecked(settings->hideErrorMessages);

    ui->checkBoxDoublePages->setChecked(settings->doublePageFullscreen);
    ui->checkBoxTrim->setChecked(settings->trimPages);
    ui->checkBoxManhwaMode->setChecked(settings->manhwaMode);

    ui->comboBoxTab->setCurrentIndex(settings->tabAdvance);
    ui->comboBoxSwipe->setCurrentIndex(settings->swipeAdvance);
    ui->comboBoxHWButton->setCurrentIndex(settings->buttonAdvance);

    ui->comboBoxMangaOrder->setCurrentIndex(settings->mangaOrder);

    setupSourcesList();
    internalChange = false;
}

void SettingsDialog::updateActiveMangasSettings(const QString &name, bool enabled)
{
    if (internalChange)
        return;

    settings->enabledMangaSources[name] = enabled;
    settings->scheduleSerialize();

    emit activeMangasChanged();
}

void SettingsDialog::updateSettings()
{
    if (internalChange)
        return;

    settings->hideErrorMessages = ui->checkBoxHideErrorMessages->isChecked();

    settings->doublePageFullscreen = ui->checkBoxDoublePages->isChecked();
    settings->trimPages = ui->checkBoxTrim->isChecked();
    settings->manhwaMode = ui->checkBoxManhwaMode->isChecked();

    settings->tabAdvance = static_cast<AdvancePageGestureDirection>(ui->comboBoxTab->currentIndex());
    settings->swipeAdvance = static_cast<AdvancePageGestureDirection>(ui->comboBoxSwipe->currentIndex());
    settings->buttonAdvance = static_cast<AdvancePageHWButton>(ui->comboBoxHWButton->currentIndex());

    auto oldOrder = settings->mangaOrder;
    settings->mangaOrder = static_cast<MangaOrderMethod>(ui->comboBoxMangaOrder->currentIndex());

    if (oldOrder != settings->mangaOrder)
        emit mangaOrderMethodChanged();

    settings->scheduleSerialize();
}

void SettingsDialog::setupSourcesList()
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
