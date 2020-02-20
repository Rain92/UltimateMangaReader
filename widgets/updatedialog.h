#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>

namespace Ui
{
class UpdateDialog;
}

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(QWidget *parent = 0);
    ~UpdateDialog();

    void setup(int maximum, const QString &msg);

    void error(const QString &msg);
    void setLabelText(const QString &msg);

    void updateProgress(int i);

signals:
    void retry();

private slots:
    void on_pushButtonRetry_clicked();

    void on_pushButtonOk_clicked();

private:
    Ui::UpdateDialog *ui;
};

#endif  // UPDATEDIALOG_H
