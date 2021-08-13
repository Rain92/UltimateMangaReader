#include "clineedit.h"

#include <QDebug>
#include <QtCore>

CLineEdit::CLineEdit(QWidget *parent) : QLineEdit(parent) {}

void CLineEdit::focusInEvent(QFocusEvent *e)
{
    QLineEdit::focusInEvent(e);
    auto ev = new QEvent(
        static_cast<QEvent::Type>(QEvent::RequestSoftwareInputPanel + 1000));
    qApp->postEvent(this->parent(), ev);
}

void CLineEdit::focusOutEvent(QFocusEvent *e)
{
    QLineEdit::focusOutEvent(e);
    auto ev = new QEvent(
        static_cast<QEvent::Type>(QEvent::CloseSoftwareInputPanel + 1000));
    qApp->postEvent(this->parent(), ev);
}
