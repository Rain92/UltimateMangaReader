#include "widgetutils.h"

#include <QFile>
#include <QGuiApplication>
#include <QPainter>
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

QIcon sourceLogo(const QString &name, const QString &iconPath, int size)
{
    QPixmap logo;
    if (!iconPath.isEmpty() && logo.load(iconPath))
        return QIcon(logo);

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::white);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    int border = qMax(2, size / 24);
    painter.setPen(QPen(Qt::black, border));
    painter.drawRoundedRect(QRectF(pixmap.rect()).adjusted(border, border, -border, -border), size / 8.0,
                            size / 8.0);
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(size * 2 / 5);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, name.left(2));
    return QIcon(pixmap);
}
