#ifndef GOTODIALOG_H
#define GOTODIALOG_H

#include <QDialog>
#include "mangadex.h"

namespace Ui {
class GotoDialog;
}

class GotoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GotoDialog(QWidget *parent = 0);
    ~GotoDialog();

    void setup(int maxchapter, int maxpage, MangaIndex currentindex);

    MangaIndex selectedindex;
    MangaIndex currentindex;

private slots:
    void on_pushButtonGoChapter_clicked();

    void on_pushButtonGoPage_clicked();

private:
    Ui::GotoDialog *ui;
};

#endif // GOTODIALOG_H
