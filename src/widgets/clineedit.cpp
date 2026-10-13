#include "clineedit.h"

CLineEdit::CLineEdit(QWidget *parent) : QLineEdit(parent) {}

void CLineEdit::focusInEvent(QFocusEvent *e)
{
    QLineEdit::focusInEvent(e);
    emit keyboardRequested();
}

// also a tap into the field that has the focus already (the keyboard was closed with its own key)
void CLineEdit::mousePressEvent(QMouseEvent *e)
{
    QLineEdit::mousePressEvent(e);
    emit keyboardRequested();
}

void CLineEdit::focusOutEvent(QFocusEvent *e)
{
    QLineEdit::focusOutEvent(e);
    emit keyboardDismissed();
}
