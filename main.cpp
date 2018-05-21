#include <QApplication>
#include "mainwidget.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWidget mw;
    mw.show();

    return a.exec();
}
