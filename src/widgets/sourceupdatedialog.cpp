#include "sourceupdatedialog.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "sizes.h"

SourceUpdateDialog::SourceUpdateDialog(QWidget *parent)
    : QDialog(parent),
      titleLabel(new QLabel(this)),
      textLabel(new QLabel(this)),
      yesButton(new QPushButton(this)),
      noButton(new QPushButton(this)),
      busy(false)
{
    setWindowFlags(Qt::Popup);
    setFixedWidth(SIZES.sourceUpdateDialogWidth);

    titleLabel->setStyleSheet("font-size: 15pt");
    titleLabel->setAlignment(Qt::AlignCenter);
    textLabel->setWordWrap(true);
    textLabel->setAlignment(Qt::AlignCenter);

    for (auto button : {yesButton, noButton})
    {
        button->setFixedHeight(SIZES.buttonSize);
        button->setFocusPolicy(Qt::NoFocus);
    }
    connect(yesButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(noButton, &QPushButton::clicked, this, &QDialog::reject);

    auto buttons = new QHBoxLayout();
    buttons->addWidget(noButton);
    buttons->addWidget(yesButton);
    auto layout = new QVBoxLayout(this);
    layout->addWidget(titleLabel);
    layout->addWidget(textLabel);
    layout->addSpacing(SIZES.buttonSize / 3);
    layout->addLayout(buttons);
}

bool SourceUpdateDialog::ask(const QString &title, const QString &text, const QString &yes, const QString &no)
{
    busy = false;
    titleLabel->setText(title);
    textLabel->setText(text);
    yesButton->setText(yes);
    noButton->setText(no);
    yesButton->show();
    noButton->show();
    adjustSize();
    return exec() == Accepted;
}

void SourceUpdateDialog::showBusy(const QString &title, const QString &text)
{
    busy = true;
    titleLabel->setText(title);
    textLabel->setText(text);
    yesButton->hide();
    noButton->hide();
    adjustSize();
    setModal(true);
    show();
    repaint();
}

void SourceUpdateDialog::finish()
{
    busy = false;
    hide();
}

void SourceUpdateDialog::closeEvent(QCloseEvent *event)
{
    if (busy)
        event->ignore();
    else
        QDialog::closeEvent(event);
}

void SourceUpdateDialog::reject()
{
    if (!busy)
        QDialog::reject();
}
