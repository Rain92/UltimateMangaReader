#include <QApplication>
#include <QScreen>
#include <QQuickView>
#include "mainwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
//    qApp->setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents, true);
//    qApp->setAttribute(Qt::AA_SynthesizeTouchForUnhandledMouseEvents, true);


//    QQuickView view;
//    view.setResizeMode(QQuickView::SizeRootObjectToView);
//    view.setSource(QUrl("qrc:///MainQMLForm.qml"));
//    view.show();


    MainWidget mainwidget;

    QFile stylesheetFile(":/resources/eink.qss");
    stylesheetFile.open(QFile::ReadOnly);
    QString styleSheet = QLatin1String(stylesheetFile.readAll());
    stylesheetFile.close();
    mainwidget.setStyleSheet(styleSheet);

    mainwidget.show();

    return app.exec();
}
