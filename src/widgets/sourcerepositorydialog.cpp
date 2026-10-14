#include "sourcerepositorydialog.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "clineedit.h"
#include "device.h"
#include "sizes.h"
#include "virtualkeyboard.h"

SourceRepositoryDialog::SourceRepositoryDialog(QWidget *parent)
    : QDialog(parent), lineEdit(new CLineEdit(this)), keyboard(new VirtualKeyboard(this))
{
    setWindowFlags(Qt::Popup);

    auto title = new QLabel("Sources repository", this);
    title->setStyleSheet("font-size: 15pt");
    title->setAlignment(Qt::AlignCenter);
    auto text = new QLabel("The address of the directory with the index.yaml of the sources. Empty: the default "
                           "one.",
                           this);
    text->setWordWrap(true);
    lineEdit->setFixedHeight(SIZES.buttonSize);

    auto defaultButton = new QPushButton(" Default ", this);
    auto cancelButton = new QPushButton(" Cancel ", this);
    auto saveButton = new QPushButton(" Save ", this);
    for (auto button : {defaultButton, cancelButton, saveButton})
    {
        button->setFixedHeight(SIZES.buttonSize);
        button->setFocusPolicy(Qt::NoFocus);
        button->setAutoDefault(false);
    }
    // the enter key of the keyboard
    saveButton->setDefault(true);
    connect(defaultButton, &QPushButton::clicked, lineEdit, &QLineEdit::clear);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveButton, &QPushButton::clicked, this, &QDialog::accept);

    // the keyboard closes itself with its key, a tap into the field brings it back
    keyboard->hide();
    keyboard->installEventFilter(this);
    connect(lineEdit, &CLineEdit::keyboardRequested, this,
            [this]()
            {
                if (Device::get().needsVirtualKeyboard())
                    keyboard->show();
            });

    auto buttons = new QHBoxLayout();
    buttons->addWidget(defaultButton);
    buttons->addStretch();
    buttons->addWidget(cancelButton);
    buttons->addWidget(saveButton);
    auto layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(text);
    layout->addWidget(lineEdit);
    layout->addLayout(buttons);
    layout->addWidget(keyboard);
}

std::optional<QString> SourceRepositoryDialog::ask(const QString &current, const QString &defaultUrl)
{
    lineEdit->setText(current);
    lineEdit->setPlaceholderText(defaultUrl);
    lineEdit->setFocus();
    place();
    if (exec() != Accepted)
        return std::nullopt;

    auto repository = lineEdit->text().trimmed();
    return repository == defaultUrl ? QString() : repository;
}

bool SourceRepositoryDialog::eventFilter(QObject *object, QEvent *event)
{
    if (object == keyboard && (event->type() == QEvent::Show || event->type() == QEvent::Hide))
        place();
    return false;
}

void SourceRepositoryDialog::place()
{
    auto parent = parentWidget();
    layout()->invalidate();
    int width = parent->width();
    int height = hasHeightForWidth() ? heightForWidth(width) : sizeHint().height();
    auto bottomLeft = parent->mapToGlobal(QPoint(0, parent->height()));
    setGeometry(bottomLeft.x(), bottomLeft.y() - height, width, height);
}
