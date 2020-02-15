#include "customgesturerecognizer.h"
#include <QMouseEvent>
#include <QWidget>
#include <QDebug>

SwipeGestureRecognizer::SwipeGestureRecognizer() :
      horizontalDirection(QSwipeGesture::NoDirection),
      verticalDirection(QSwipeGesture::NoDirection),
      swipeAngle(0),
      state(NoGesture), velocityValue(0)
{}

QGesture *SwipeGestureRecognizer::create(QObject *target)
{
    if (target && target->isWidgetType()) {
        static_cast<QWidget *>(target)->setAttribute(Qt::WA_AcceptTouchEvents);
    }
    return new QSwipeGesture;
}

QGestureRecognizer::Result SwipeGestureRecognizer::recognize(QGesture *state,
                                                              QObject *,
                                                              QEvent *event)
{
    QSwipeGesture *q = static_cast<QSwipeGesture *>(state);

    QGestureRecognizer::Result result = QGestureRecognizer::Ignore;
    const QMouseEvent *ev = static_cast<const QMouseEvent *>(event);

    switch (event->type()) {
    case QEvent::MouseButtonPress : {
//        qDebug() << "touch press";
        this->velocityValue = 1;
        this->time.start();
        this->state = State::Started;
        q->setHotSpot(ev->screenPos());
        this->startPosition = ev->screenPos().toPoint();
        result = QGestureRecognizer::MayBeGesture;
        break;
    }
    case QEvent::MouseButtonRelease: {
//        qDebug() << "touch release";

        if (q->state() != Qt::NoGesture) {
            result = QGestureRecognizer::FinishGesture;
        } else {
            result = QGestureRecognizer::CancelGesture;
        }
        break;
    }
    case QEvent::MouseMove: {
        if (this->state == State::NoGesture)
            result = QGestureRecognizer::CancelGesture;
        else {
            auto mousepos = ev->screenPos().toPoint();


            int xDistance = mousepos.x() - this->startPosition.x();
            int yDistance = mousepos.y() - this->startPosition.y();

            const int distance = xDistance >= yDistance ? xDistance : yDistance;
            int elapsedTime = this->time.restart();
            if (!elapsedTime)
                elapsedTime = 1;
            this->velocityValue = 0.9 * this->velocityValue + (qreal) distance / elapsedTime;
            this->swipeAngle = QLineF(this->startPosition, mousepos).angle();


            if (qAbs(xDistance) > MoveThreshold || qAbs(yDistance) > MoveThreshold) {
                // measure the distance to check if the direction changed

                this->startPosition = ev->screenPos().toPoint();
                q->setSwipeAngle(this->swipeAngle);
//                qDebug() << this->swipeAngle;

                result = QGestureRecognizer::TriggerGesture;
                // QTBUG-46195, small changes in direction should not cause the gesture to be canceled.
                if (this->verticalDirection == QSwipeGesture::NoDirection || qAbs(yDistance) > directionChangeThreshold) {
                    const QSwipeGesture::SwipeDirection vertical = yDistance > 0
                        ? QSwipeGesture::Down : QSwipeGesture::Up;
                    if (this->verticalDirection != QSwipeGesture::NoDirection && this->verticalDirection != vertical)
                        result = QGestureRecognizer::CancelGesture;
                    this->verticalDirection = vertical;
                }
                if (this->horizontalDirection == QSwipeGesture::NoDirection || qAbs(xDistance) > directionChangeThreshold) {
                    const QSwipeGesture::SwipeDirection horizontal = xDistance > 0
                        ? QSwipeGesture::Right : QSwipeGesture::Left;
                    if (this->horizontalDirection != QSwipeGesture::NoDirection && this->horizontalDirection != horizontal)
                        result = QGestureRecognizer::CancelGesture;
                    this->horizontalDirection = horizontal;
                }
            } else {
                if (q->state() != Qt::NoGesture)
                    result = QGestureRecognizer::TriggerGesture;
                else
                    result = QGestureRecognizer::MayBeGesture;
            }
        }
        break;
    }
    default:
        break;
    }
    return result;
}

void SwipeGestureRecognizer::reset(QGesture *state)
{
    this->verticalDirection = this->horizontalDirection = QSwipeGesture::NoDirection;
    this->swipeAngle = 0;

    this->startPosition = QPoint();
    this->state = State::NoGesture;
    this->velocityValue = 0;
    this->time.invalidate();

    QGestureRecognizer::reset(state);
}


TapGestureRecognizer::TapGestureRecognizer()
{
}

QGesture *TapGestureRecognizer::create(QObject *target)
{
    if (target && target->isWidgetType()) {
        static_cast<QWidget *>(target)->setAttribute(Qt::WA_AcceptTouchEvents);
    }
    return new QTapGesture;
}

QGestureRecognizer::Result TapGestureRecognizer::recognize(QGesture *state,
                                                            QObject *,
                                                            QEvent *event)
{
    QTapGesture *q = static_cast<QTapGesture *>(state);

    const QMouseEvent *ev = static_cast<const QMouseEvent *>(event);

    QGestureRecognizer::Result result = QGestureRecognizer::CancelGesture;

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        this->position = ev->screenPos();
        q->setHotSpot(this->position);
        timer.start();
        this->pressed = true;
        result = QGestureRecognizer::Ignore;
        qDebug() << "pressed!";
        break;
    }
    case QEvent::MouseMove:
        break;
    case QEvent::MouseButtonRelease: {
        if (q->state() == Qt::NoGesture && this->pressed) {
            this->pressed = false;
            qDebug() << "released!";
            auto p = ev->screenPos().toPoint();
            QPoint delta = p - this->position.toPoint();
            if (delta.manhattanLength() <= TAPRADIUS && timer.elapsed() < TIMEOUT)
            {
                result = QGestureRecognizer::FinishGesture;
            }
            else
                result = QGestureRecognizer::CancelGesture;
        }
        break;
    }
    default:
        result = QGestureRecognizer::Ignore;
        break;
    }
    return result;
}

void TapGestureRecognizer::reset(QGesture *state)
{
    this->pressed = false;
    this->position = QPointF();

    QGestureRecognizer::reset(state);
}
