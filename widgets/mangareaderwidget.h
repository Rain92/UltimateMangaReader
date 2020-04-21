#ifndef MANGAREADERWIDGET_H
#define MANGAREADERWIDGET_H

#include <QGesture>

#include "customgesturerecognizer.h"
#include "enums.h"
#include "gotodialog.h"
#include "mangainfo.h"
#include "readingprogress.h"
#include "settings.h"
#include "sizes.h"

namespace Ui
{
class MangaReaderWidget;
}

class MangaReaderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MangaReaderWidget(QWidget *parent = nullptr);
    ~MangaReaderWidget();

    void showImage(const QString &path);
    void updateCurrentIndex(const ReadingProgress &progress);

    void setFrontLightPanelState(int lightmin, int lightmax, int light, int comflightmin, int comflightmax,
                                 int comflight);
    void setFrontLightPanelState(int light, int comflight);

    void addImageToCache(const QString &path);
    void updateMenuBar();
    void setSettings(Settings *settings);

signals:
    void changeView(WidgetTab page);
    void advancPageClicked(PageTurnDirection direction);
    void closeApp();
    void back();
    void frontlightchanged(int light, int comflight);
    void gotoIndex(const MangaIndex &mangaIndex);

private slots:
    void on_pushButtonReaderHome_clicked();
    void on_pushButtonReaderBack_clicked();
    void on_pushButtonReaderFavorites_clicked();

    void on_horizontalSliderLight_valueChanged(int value);

    void on_horizontalSliderComfLight_valueChanged(int value);

    void on_pushButtonReaderGoto_clicked();

protected:
    bool event(QEvent *event) override;

private:
    bool gestureEvent(QGestureEvent *event);
    bool buttonPressEvent(QKeyEvent *event);

    Ui::MangaReaderWidget *ui;

    QQueue<QPair<QSharedPointer<QPixmap>, QString>> imgcache;

    GotoDialog *gotodialog;

    Settings *settings;

    void adjustUI();

    int searchCache(const QString &path) const;

    void showMenuBar(bool show);
};

#endif  // MANGAREADERWIDGET_H
