#include <QtCore>

#include "mainwidget.h"

int main(int argc, char *argv[])
{
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    MainWidget mainwidget;

    QApplication::setStyle("windows");
    QFile stylesheetFile(":/eink.qss");
    stylesheetFile.open(QFile::ReadOnly);
    mainwidget.setStyleSheet(stylesheetFile.readAll());
    stylesheetFile.close();

    mainwidget.show();

    return app.exec();
}
