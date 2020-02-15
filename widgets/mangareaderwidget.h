#ifndef MANGAREADERWIDGET_H
#define MANGAREADERWIDGET_H

#include <QWidget>
#include <QPair>
#include <QGesture>

#include "mangainfo.h"
#include "gotodialog.h"
#include "configs.h"

namespace Ui {
class MangaReaderWidget;
}

class MangaReaderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MangaReaderWidget(QWidget *parent = 0);
    ~MangaReaderWidget();

    void showImage(const QString &path);
    void updateReaderLabels(QSharedPointer<MangaInfo> info);

    void setFrontLightPanelState(int lightmin, int lightmax, int light, int comflightmin, int comflightmax, int comflight);
    void setFrontLightPanelState(int light, int comflight);


public slots:
    void addImageToCache(const QString &path);
    void updateTime();

signals:
    void changeView(int page);
    void advancPageClicked(bool direction);
    void closeApp();
    void back();
    void frontlightchanged(int light, int comflight);
    void gotoIndex(MangaIndex index);
    void enableVirtualKeyboard(bool enable);

private slots:
    void on_pushButtonReaderHome_clicked();
    void on_pushButtonReaderBack_clicked();
    void on_pushButtonReaderClose_clicked();

    void on_horizontalSliderLight_valueChanged(int value);

    void on_horizontalSliderComfLight_valueChanged(int value);

    void on_pushButtonReaderGoto_clicked();


protected:
    bool event(QEvent *event) override;


private:
    bool gestureEvent(QGestureEvent *event);


    Ui::MangaReaderWidget *ui;

    bool pagechanging;

    void adjustSizes();


    void setBatteryIcon();
    QPair<int, bool> getBatteryState();

    void showMenuBar(bool show);

    QList<QPixmap *> imgcache;
    QList<QString> imgcachepaths;

    QSharedPointer<MangaInfo> currentmanga;

    QPixmap batteryicons[4];

    GotoDialog *gotodialog;
};

#endif // MANGAREADERWIDGET_H
