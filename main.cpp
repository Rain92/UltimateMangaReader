#include <QApplication>
#include <QScreen>

#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#include "mainwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWidget mainwidget;

#ifdef KOBO
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    auto dev = KoboPlatformFunctions::getKoboDeviceDescriptor();
    mainwidget.resize(dev.width, dev.height);
#endif

    QFile stylesheetFile(":/resources/eink.qss");
    stylesheetFile.open(QFile::ReadOnly);
    QString styleSheet = QLatin1String(stylesheetFile.readAll());
    stylesheetFile.close();
    mainwidget.setStyleSheet(styleSheet);

    mainwidget.show();

    return app.exec();
}
