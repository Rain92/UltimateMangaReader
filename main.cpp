#include <QApplication>
#include <QScreen>
#include "mainwidget.h"

#ifndef WINDOWS
#include "Application.h"
#include "Log.h"
#endif

int main(int argc, char *argv[])
{
#ifndef WINDOWS
    Application application(argc, argv);
    application.init();
    g_pLog->setLevel(2);

#else
    QApplication application(argc, argv);
#endif

    MainWidget mainwidget;

    QFile File(":/resources/eink.qss");
    File.open(QFile::ReadOnly);
    QString StyleSheet = QLatin1String(File.readAll());
    mainwidget.setStyleSheet(StyleSheet);

    mainwidget.show();

    return application.exec();
}
