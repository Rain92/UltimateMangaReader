#include "widgetutils.h"

#include <QGuiApplication>
#include <QScroller>
#include <atomic>

// read by the threads that generate cover thumbnails
static std::atomic<double> currentWindowPixelRatio{0};

qreal windowPixelRatio()
{
    double ratio = currentWindowPixelRatio.load(std::memory_order_relaxed);
    return ratio > 0 ? ratio : qGuiApp->devicePixelRatio();
}

void setWindowPixelRatio(qreal ratio)
{
    currentWindowPixelRatio.store(ratio, std::memory_order_relaxed);
}

void activateScroller(QAbstractScrollArea* pArea)
{
    if (pArea)
    {
        QScroller* scroller = QScroller::scroller(pArea);
        // properties won't be set for some reason; has to be hardfixed in the qt source
        QScrollerProperties prop;
        prop.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0);
        prop.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
                             QScrollerProperties::OvershootAlwaysOff);
        prop.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
                             QScrollerProperties::OvershootAlwaysOff);

        prop.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0);
        prop.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor, 0);

        scroller->setScrollerProperties(prop);
        scroller->grabGesture(pArea->viewport(), QScroller::LeftMouseButtonGesture);
    }
}
