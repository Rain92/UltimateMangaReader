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
    void updateActiveMangasSettings(const QString &name, bool enabled);

    void setupSourcesList();

    Settings *settings;
};

#endif  // SETTINGSWIDGET_H
