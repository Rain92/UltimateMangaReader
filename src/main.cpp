#include <QtCore>

#include "mainwidget.h"
#include "perftimer.h"
#include "stacktrace.h"

int main(int argc, char *argv[])
{
    registerBacktraceHandlers();

    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    PerfTimer startup("startup");

    MainWidget mainwidget;
    startup.lap("main widget");

    // after the widgets are created: applying it first makes creating them more than twice as slow
    QApplication::setStyle("windows");
    QFile stylesheetFile(":/eink.qss");
    stylesheetFile.open(QFile::ReadOnly);
    mainwidget.setStyleSheet(stylesheetFile.readAll());
    stylesheetFile.close();
    startup.lap("style");

    mainwidget.show();
    startup.lap("show");

    startStallWatchdog(&app);
    QTimer::singleShot(0, &app, [&startup]() { startup.lap("first events"); });

    return app.exec();
}
