#include <QApplication>
#include <QScreen>

#ifdef KOBO
#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#endif

#include "mainwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWidget mainwidget;

#ifdef KOBO
    auto dev = KoboPlatformFunctions::getKoboDeviceDescriptor();
    mainwidget.resize(dev.width, dev.height);
#endif

    QFile stylesheetFile(":/eink.qss");
    stylesheetFile.open(QFile::ReadOnly);
    QString styleSheet(stylesheetFile.readAll());
    stylesheetFile.close();
    mainwidget.setStyleSheet(styleSheet);

    mainwidget.show();

    return app.exec();
}
