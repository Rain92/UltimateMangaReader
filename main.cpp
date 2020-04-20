#include <QApplication>
#include <QScreen>

#include "sizes.h"
#ifdef KOBO
#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#endif

#include "mainwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#ifdef KOBO
    auto dev = KoboPlatformFunctions::getKoboDeviceDescriptor();
    SIZES.setDPI(dev.dpi);
#endif

    MainWidget mainwidget;

#ifdef KOBO
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
