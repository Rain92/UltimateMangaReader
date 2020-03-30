#ifndef ERRORMESSAGEWIDGET_H
#define ERRORMESSAGEWIDGET_H

#include <QTimer>
#include <QWidget>

#include "sizes.h"

namespace Ui
{
class ErrorMessageWidget;
}

class ErrorMessageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ErrorMessageWidget(QWidget *parent = nullptr);
    ~ErrorMessageWidget();

    void showError(const QString &message);
    void setTimeout(int timeout);

private slots:
    void on_buttonHide_clicked();

private:
    Ui::ErrorMessageWidget *ui;
    QTimer hideTimer;
    int timeout;
};

#endif  // ERRORMESSAGEWIDGET_H
