#pragma once

#include <QLineEdit>

// A line edit that asks for the on-screen keyboard while it has the focus.
class CLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    CLineEdit(QWidget *parent = nullptr);

signals:
    void keyboardRequested();
    void keyboardDismissed();

protected:
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
};
