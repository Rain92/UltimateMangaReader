#pragma once

#include <QDialog>

#include "enums.h"
#include "settings.h"
#include "sizes.h"
#include "sourceregistry.h"
#include "utils.h"

namespace Ui
{
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT
    Q_ENUM(AdvancePageGestureDirection)
    Q_ENUM(AdvancePageHWButton)

public:
    SettingsDialog(Settings *settings, SourceRegistry *sources, QWidget *parent = nullptr);
    ~SettingsDialog();

    void open() override;

signals:
    void mangaOrderMethodChanged();
    void ditheringMethodChanged();

private slots:
    void on_pushButtonOk_clicked();

private:
    Ui::SettingsDialog *ui;
    Settings *settings;
    bool internalChange;

    void resetUI();
    void adjustUI();

    void updateSettings();
};
