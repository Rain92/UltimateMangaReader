#ifndef CLEARCACHEDIALOG_H
#define CLEARCACHEDIALOG_H

#include <QDialog>

namespace Ui {
class ClearCacheDialog;
}

class ClearCacheDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClearCacheDialog(QWidget *parent = 0);
    ~ClearCacheDialog();

    void getCacheSize();

private slots:
    void on_pushButtonCancel_clicked();

    void on_pushButtonClear1_clicked();

    void on_pushButtonClear2_clicked();

    void on_pushButtonClear3_clicked();

signals:
    bool clearCache(int level);

private:
    Ui::ClearCacheDialog *ui;
};

#endif // CLEARCACHEDIALOG_H
