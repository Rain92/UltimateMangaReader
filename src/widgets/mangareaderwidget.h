#pragma once

#include <QFuture>
#include <QGesture>
#include <QThreadPool>

#include <memory>

#include "customgesturerecognizer.h"
#include "enums.h"
#include "gotodialog.h"
#include "mangainfo.h"
#include "readingprogress.h"
#include "settings.h"
#include "sizes.h"
#include "utils.h"

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

    // Decodes a page that is on disk into the cache in the background (the pages next to the current one).
    void prefetchImage(const QString &path);
    bool addImageToCache(const QString &path, QSharedPointer<QImage> img);
    void updateMenuBar();
    void setSettings(Settings *settings);
    QDialog *gotoDialog() const { return gotodialog; }

    void clearCache();

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
    void gotoDialogAccepted();

protected:
    bool event(QEvent *event) override;

private:
    bool gestureEvent(QGestureEvent *event);
    bool buttonPressEvent(QKeyEvent *event);

    Ui::MangaReaderWidget *ui;

    QQueue<QPair<QSharedPointer<QImage>, QString>> imgcache;

    // A page turn to a cached page doesn't have to decode it first (40-150 ms on the device). One page is
    // decoded at a time, the gui thread waits for it if it's the one to show.
    struct Prefetch
    {
        QString path;
        QImage image;
    };
    QThreadPool prefetchPool;
    std::shared_ptr<Prefetch> prefetch;
    QFuture<void> prefetchFuture;
    QStringList prefetchQueue;
    void startPrefetch();
    void prefetchFinished(const std::shared_ptr<Prefetch> &job);
    bool takePrefetchedImage(const QString &path);

    GotoDialog *gotodialog;

    Settings *settings;

    void adjustUI();

    int searchCache(const QString &path) const;
    bool loadImageToCache(const QString &path);

    void showMenuBar(bool show);
    void checkMem();
};
