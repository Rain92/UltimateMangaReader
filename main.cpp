#include <QApplication>
#include "mainwidget.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWidget mw;
    QFile File(":/resources/eink.qss");
    File.open(QFile::ReadOnly);
    QString StyleSheet = QLatin1String(File.readAll());
    mw.setStyleSheet(StyleSheet);
    mw.show();

    return a.exec();
}
