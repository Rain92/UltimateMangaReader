#include "errormessagewidget.h"

#include "ui_errormessagewidget.h"

ErrorMessageWidget::ErrorMessageWidget(QWidget* parent)
    : QWidget(parent), ui(new Ui::ErrorMessageWidget), hideTimer(), timeout(3000)
{
    ui->setupUi(this);

    ui->labelErrorImage->setFixedSize(SIZES.buttonSize, SIZES.buttonSize);

    ui->buttonHide->setMinimumHeight(SIZES.buttonSize);

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

    this->setFixedHeight(ui->labelErrorMessage->sizeHint().height());
}

void ErrorMessageWidget::setTimeout(int timeout)
{
    this->timeout = timeout;
}
