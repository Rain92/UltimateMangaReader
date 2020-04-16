#include "errormessagewidget.h"

#include "ui_errormessagewidget.h"

ErrorMessageWidget::ErrorMessageWidget(QWidget* parent)
    : QWidget(parent), ui(new Ui::ErrorMessageWidget), hideTimer(), timeout(5000)
{
    ui->setupUi(this);

    QPixmap pixmap(":/images/icons/error.png");
    ui->labelErrorImage->setPixmap(
        pixmap.scaled(ui->labelErrorImage->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    ui->buttonHide->setMinimumHeight(errormessagewidgetheight);
    this->setMinimumHeight(errormessagewidgetheight);
    this->setMaximumHeight(errormessagewidgetheight);

    this->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    ui->buttonHide->setFocusPolicy(Qt::FocusPolicy::NoFocus);

    hideTimer.setSingleShot(true);
    QObject::connect(&this->hideTimer, &QTimer::timeout, this, &QWidget::hide);

    this->hide();
}

ErrorMessageWidget::~ErrorMessageWidget()
{
    delete ui;
}

void ErrorMessageWidget::on_buttonHide_clicked()
{
    this->hide();
}

void ErrorMessageWidget::showError(const QString& message)
{
    ui->labelErrorMessage->setText(message);
    this->raise();
    this->show();

    if (timeout > 0)
        hideTimer.start(timeout);
}

void ErrorMessageWidget::setTimeout(int timeout)
{
    this->timeout = timeout;
}
