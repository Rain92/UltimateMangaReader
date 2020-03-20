#ifndef CLINEEDIT_H
#define CLINEEDIT_H

#include <QEvent>
#include <QLineEdit>

class CLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    CLineEdit(QWidget *parent = 0);

protected:
    virtual void focusInEvent(QFocusEvent *e);
    virtual void focusOutEvent(QFocusEvent *e);
};

#endif  // CLINEEDIT_H
