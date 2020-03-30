#ifndef NUMPADWIDGET_H
#define NUMPADWIDGET_H

#include <QAbstractButton>
#include <QIcon>
#include <QKeyEvent>

#include "sizes.h"

namespace Ui
{
class NumpadWidget;
}

class NumpadWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NumpadWidget(QWidget *parent = 0);
    ~NumpadWidget();

    void setupButtons();

public slots:
    void numButtonPressed(QAbstractButton *button);

private:
    Ui::NumpadWidget *ui;
};

#endif  // NUMPADWIDGET_H
