#ifndef SCREENSAVERDIALOG_H
#define SCREENSAVERDIALOG_H

#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLayout>

#include "enums.h"
#include "imageprocessingnative.h"
#include "staticsettings.h"

namespace Ui
{
class ScreensaverDialog;
}

class ScreensaverDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ScreensaverDialog(QWidget *parent);
    ~ScreensaverDialog();

    void showRandomScreensaver();

protected:
    bool event(QEvent *event) override;

private:
    Ui::ScreensaverDialog *ui;
};

#endif  // SCREENSAVERDIALOG_H
