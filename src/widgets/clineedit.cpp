#include "clineedit.h"

#include <QDebug>
#include <QMouseEvent>
#include <QtCore>

CLineEdit::CLineEdit(QWidget *parent) : QLineEdit(parent) {}

void CLineEdit::focusInEvent(QFocusEvent *e)
{
    QLineEdit::focusInEvent(e);
    auto ev = new QEvent(
        static_cast<QEvent::Type>(QEvent::RequestSoftwareInputPanel + 1000));
    qApp->postEvent(this->parent(), ev);
}

// also a tap into the field that has the focus already (the keyboard was closed with its own key)
void CLineEdit::mousePressEvent(QMouseEvent *e)
{
    QLineEdit::mousePressEvent(e);
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
