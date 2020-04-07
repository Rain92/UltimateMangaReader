#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QWidget>

#include "settings.h"

namespace Ui
{
class SettingsWidget;
}

class SettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWidget(QWidget *parent = nullptr);
    ~SettingsWidget();

    void setSettings(Settings *settings);

    void updateUI();

signals:
    void activeMangasChanged();

private:
    Ui::SettingsWidget *ui;

    void updateSettings();
    void updateActiveMangasSettings();

    void setupSourcesList();

    Settings *settings;
};

#endif  // SETTINGSWIDGET_H
