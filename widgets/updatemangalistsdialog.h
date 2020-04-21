#ifndef UPDATEMANGALISTSDIALOG_H
#define UPDATEMANGALISTSDIALOG_H

#include <QCheckBox>
#include <QDialog>

#include "settings.h"
#include "sizes.h"
#include "updateprogresstoken.h"

namespace Ui
{
class UpdateMangaListsDialog;
}

class UpdateMangaListsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateMangaListsDialog(Settings *settings, QWidget *parent);
    ~UpdateMangaListsDialog();

    void open() override;

private slots:
    void on_pushButtonOk_clicked();

    void on_pushButtonCancel_clicked();
    void on_pushButtonFinish_clicked();

signals:
    void updateClicked(QSharedPointer<UpdateProgressToken> token);

private:
    Ui::UpdateMangaListsDialog *ui;
    Settings *settings;
    QSharedPointer<UpdateProgressToken> progressToken;

    void resetUI();
    void setupSourcesList();
    void updateFinished();
    void updateProgress();
    void updateError(const QString &message);
};

#endif  // UPDATEMANGALISTSDIALOG_H
