#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

#include "settings.h"
#include "sizes.h"

namespace Ui
{
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    void setSettings(Settings *settings);

    void resetUI();

signals:
    void activeMangasChanged();

private slots:
    void on_pushButtonOk_clicked();

private:
    Ui::SettingsDialog *ui;
    Settings *settings;

    void adjustSizes();

    void updateSettings();
    void updateActiveMangasSettings(const QString &name, bool enabled);

    void setupSourcesList();
};

#endif  // SETTINGSDIALOG_H
