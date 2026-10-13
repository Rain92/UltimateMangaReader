#pragma once

#include <QDialog>

#include "sizes.h"
#include "sourcelistmodel.h"
#include "updateprogresstoken.h"

namespace Ui
{
class UpdateMangaListsDialog;
}

class UpdateMangaListsDialog : public QDialog
{
    Q_OBJECT

public:
    UpdateMangaListsDialog(SourceRegistry *sources, QWidget *parent);
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
    // the enabled ones, checked are the ones to update
    SourceListModel *sources;
    QSharedPointer<UpdateProgressToken> progressToken;
    // the sources an unfinished update completed are unchecked when the dialog is opened again
    bool updateCompleted;

    void resetUI();
    void updateFinished(const QString &message);

    void updateProgress();
    void updateError(const QString &message);
};
