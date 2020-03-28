#ifndef GOTODIALOG_H
#define GOTODIALOG_H

#include <QDialog>
#include <QResizeEvent>

#include "mangaindex.h"

namespace Ui
{
class GotoDialog;
}

class GotoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GotoDialog(QWidget *parent = 0);
    ~GotoDialog();

    void setup(MangaIndex currentindex, int maxchapter, int maxpage);

    void moveCursorRight(int steps = 4);

    MangaIndex selectedindex;
    MangaIndex currentindex;

protected:
    bool eventFilter(QObject *obj, QEvent *ev);

protected slots:

private slots:
    void on_pushButtonGoChapter_clicked();

    void on_pushButtonGoPage_clicked();

private:
    Ui::GotoDialog *ui;
};

#endif  // GOTODIALOG_H
