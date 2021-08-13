#include "numpadwidget.h"

#include "ui_numpadwidget.h"

NumpadWidget::NumpadWidget(QWidget *parent) : QWidget(parent), ui(new Ui::NumpadWidget)
{
    ui->setupUi(this);

    setupButtons();
}

NumpadWidget::~NumpadWidget()
{
    delete ui;
}

void NumpadWidget::setupButtons()
{
    foreach (QAbstractButton *btn, ui->buttonGroupAllButtons->buttons())
    {
        if (btn->text() == "<-")
        {
            btn->setProperty("key", Qt::Key_Backspace);
            QIcon icon = QIcon(
                QPixmap(":/images/icons/backspace.png").scaledToHeight(SIZES.buttonSize, Qt::SmoothTransformation));
            btn->setIconSize(QSize(SIZES.buttonSize * 0.5, SIZES.buttonSize * 0.5));
            btn->setIcon(icon);
            btn->setText("");
        }
        else if (btn->text() == "OK")
            btn->setProperty("key", Qt::Key_Enter);
        else
            btn->setProperty("key", Qt::Key_0 + btn->text().trimmed().toInt());

        btn->setFocusPolicy(Qt::NoFocus);
        btn->setFixedHeight(SIZES.buttonSize);
        btn->setFixedWidth(SIZES.buttonSize * 2);
    }

    QObject::connect(ui->buttonGroupAllButtons,
                     static_cast<void (QButtonGroup::*)(QAbstractButton *)>(&QButtonGroup::buttonPressed),
                     this, &NumpadWidget::numButtonPressed);
}

void NumpadWidget::numButtonPressed(QAbstractButton *button)
{
    Qt::Key key = (Qt::Key)button->property("key").toInt();

    QString repr = QKeySequence(key).toString();

    QWidget *target = qApp->focusWidget();

    QKeyEvent *pressEvent = new QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier, repr);
    QKeyEvent *releaseEvent = new QKeyEvent(QEvent::KeyRelease, key, Qt::NoModifier, repr);
    qApp->postEvent(target, pressEvent);
    qApp->postEvent(target, releaseEvent);
}
