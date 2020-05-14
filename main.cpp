#include <QApplication>
#include <QScreen>
#include <QtCore>

#include "sizes.h"
#ifdef KOBO
#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#endif

#include "mainwidget.h"

int main(int argc, char *argv[])
{
#ifndef KOBO
//    qputenv("QT_SCALE_FACTOR", "1.4567");  // debug
#endif

    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    MainWidget mainwidget;

#ifdef KOBO
    auto dev = KoboPlatformFunctions::getKoboDeviceDescriptor();
    mainwidget.resize(dev.width, dev.height);
#endif
    qDebug() << qApp->devicePixelRatio();

    QFile stylesheetFile(":/eink.qss");
    stylesheetFile.open(QFile::ReadOnly);
    QString styleSheet(stylesheetFile.readAll());
    stylesheetFile.close();
    mainwidget.setStyleSheet(styleSheet);

    mainwidget.show();

    return app.exec();
}
