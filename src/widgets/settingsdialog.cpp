#include "settingsdialog.h"

#include <QScrollBar>

#include "sourcechecklist.h"
#include "ui_settingsdialog.h"
#include "widgetutils.h"

SettingsDialog::SettingsDialog(Settings *settings, SourceRegistry *sources, QWidget *parent)
    : QDialog(parent), ui(new Ui::SettingsDialog), settings(settings), internalChange(false)
{
    ui->setupUi(this);
    adjustUI();
    setWindowFlags(Qt::Popup);

    for (auto item : this->findChildren<QComboBox *>())
        QObject::connect(item, &QComboBox::currentIndexChanged, this, &SettingsDialog::updateSettings);

    for (auto item : this->findChildren<QCheckBox *>())
        QObject::connect(item, &QCheckBox::clicked, this, &SettingsDialog::updateSettings);

    // a source is enabled or disabled right away
    auto sourceList = new SourceCheckList(new SourceListModel(sources, SourceListModel::EnableSources, this),
                                          ui->frameMangaSources);
    sourceList->layout()->setSpacing(ui->frameMangaSources->layout()->spacing());
    ui->frameMangaSources->layout()->addWidget(sourceList);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::adjustUI()
{
    ui->labelTitle->setStyleSheet("font-size: 15pt");
    ui->pushButtonOk->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonSourcesRepository->setFixedHeight(SIZES.buttonSize);
    activateScroller(ui->scrollArea);
}

void SettingsDialog::open()
{
    resetUI();
    // it keeps where it was scrolled to the last time
    ui->scrollArea->verticalScrollBar()->setValue(0);
    QRect screenGeometry = this->parentWidget()->geometry();
    int x = (screenGeometry.width() - this->width()) / 2 + this->parentWidget()->x();
    int y = (screenGeometry.height() - this->height()) / 2 + this->parentWidget()->y();
    this->setMaximumHeight(this->parentWidget()->height());
    this->move(x, y);
    QDialog::open();
}

void SettingsDialog::resetUI()
{
    internalChange = true;
    ui->checkBoxHideErrorMessages->setChecked(settings->hideErrorMessages);
    ui->checkBoxStandby->setChecked(settings->standbyWhenIdle);

    ui->comboBoxDoublePageMode->setCurrentIndex(settings->doublePageMode);
    ui->checkBoxTrim->setChecked(settings->trimPages);
    ui->checkBoxManhwaMode->setChecked(settings->manhwaMode);

    ui->comboBoxDithering->setCurrentIndex(settings->ditheringMode == NoDithering ? 0 : 1);

    ui->comboBoxTab->setCurrentIndex(settings->tabAdvance);
    ui->comboBoxSwipe->setCurrentIndex(settings->swipeAdvance);
    ui->comboBoxHWButton->setCurrentIndex(settings->buttonAdvance);

    ui->comboBoxMangaOrder->setCurrentIndex(settings->mangaOrder);

    // a url has no spaces where it could wrap
    auto repository = settings->sourcesRepositoryUrl().replace("/", "/\u200B");
    ui->labelSourcesRepository->setText(settings->sourcesRepository.isEmpty() ? repository + " (default)"
                                                                               : repository);

    internalChange = false;
}

void SettingsDialog::updateSettings()
{
    if (internalChange)
        return;

    settings->hideErrorMessages = ui->checkBoxHideErrorMessages->isChecked();
    settings->standbyWhenIdle = ui->checkBoxStandby->isChecked();

    settings->doublePageMode = static_cast<DoublePageMode>(ui->comboBoxDoublePageMode->currentIndex());
    settings->trimPages = ui->checkBoxTrim->isChecked();
    settings->manhwaMode = ui->checkBoxManhwaMode->isChecked();

    auto oldDitheringMode = settings->ditheringMode;

    settings->ditheringMode = ui->comboBoxDithering->currentIndex() == 0 ? NoDithering : SWDithering;

    if (oldDitheringMode != settings->ditheringMode)
        emit ditheringMethodChanged();

    settings->tabAdvance = static_cast<AdvancePageGestureDirection>(ui->comboBoxTab->currentIndex());
    settings->swipeAdvance = static_cast<AdvancePageGestureDirection>(ui->comboBoxSwipe->currentIndex());
    settings->buttonAdvance = static_cast<AdvancePageHWButton>(ui->comboBoxHWButton->currentIndex());

    auto oldMangaOrder = settings->mangaOrder;
    settings->mangaOrder = static_cast<MangaOrderMethod>(ui->comboBoxMangaOrder->currentIndex());

    if (oldMangaOrder != settings->mangaOrder)
        emit mangaOrderMethodChanged();

    settings->scheduleSerialize();
}

void SettingsDialog::on_pushButtonOk_clicked()
{
    close();
}

void SettingsDialog::on_pushButtonSourcesRepository_clicked()
{
    close();
    emit sourcesRepositoryEditRequested();
}
