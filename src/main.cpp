#include <QtCore>

#include "mainwidget.h"
#include "stacktrace.h"

int main(int argc, char *argv[])
{
    registerBacktraceHandlers();

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
