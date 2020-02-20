#include <QApplication>
#include <QQuickView>
#include <QScreen>

#include "mainwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#ifdef KOBO
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
#endif

    MainWidget mainwidget;

    QFile stylesheetFile(":/resources/eink.qss");
    stylesheetFile.open(QFile::ReadOnly);
    QString styleSheet = QLatin1String(stylesheetFile.readAll());
    stylesheetFile.close();
    mainwidget.setStyleSheet(styleSheet);

    mainwidget.show();

    return app.exec();
}
