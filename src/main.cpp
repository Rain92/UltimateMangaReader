#include <QApplication>
#include <QtCore>

#include "mainwidget.h"
#include "perftimer.h"
#include "stacktrace.h"

int main(int argc, char *argv[])
{
    registerBacktraceHandlers();

    QApplication app(argc, argv);

    PerfTimer startup("startup");

    MainWidget mainwidget;
    startup.lap("main widget");

    // after the widgets are created: applying it first makes creating them more than twice as slow
    QApplication::setStyle("windows");
    QFile stylesheetFile(":/eink.qss");
    if (stylesheetFile.open(QFile::ReadOnly))
        mainwidget.setStyleSheet(QString::fromUtf8(stylesheetFile.readAll()));
    else
        qDebug() << "Can't load the stylesheet";
    startup.lap("style");

    mainwidget.show();
    startup.lap("show");

    startStallWatchdog(&app);
    QTimer::singleShot(0, &app, [&startup]() { startup.lap("first events"); });

    return app.exec();
}
