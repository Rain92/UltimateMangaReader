#ifndef NUMPADWIDGET_H
#define NUMPADWIDGET_H

#include <QtCore>
#include <QWidget>
#include <QAbstractButton>

namespace Ui {
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

#endif // NUMPADWIDGET_H
