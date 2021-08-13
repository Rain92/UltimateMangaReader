#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

#include "enums.h"
#include "settings.h"
#include "sizes.h"
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
    explicit SettingsDialog(Settings *settings, QWidget *parent = nullptr);
    ~SettingsDialog();

    void open() override;

signals:
    void activeMangasChanged();
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
    void updateActiveMangasSettings(const QString &name, bool enabled);

    void setupSourcesList();
};

#endif  // SETTINGSDIALOG_H
