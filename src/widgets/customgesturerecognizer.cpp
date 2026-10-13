#include "customgesturerecognizer.h"

#include <QLineF>
#include <QMouseEvent>

SwipeGestureRecognizer::SwipeGestureRecognizer()
    : horizontalDirection(QSwipeGesture::NoDirection),
      verticalDirection(QSwipeGesture::NoDirection),
      startPosition(),
      state(NoGesture)
{
}

QGesture *SwipeGestureRecognizer::create(QObject *)
{
    return new QSwipeGesture;
}

QGestureRecognizer::Result SwipeGestureRecognizer::recognize(QGesture *state, QObject *, QEvent *event)
{
    QSwipeGesture *q = static_cast<QSwipeGesture *>(state);

    QGestureRecognizer::Result result = QGestureRecognizer::Ignore;
    const QMouseEvent *ev = static_cast<const QMouseEvent *>(event);

    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        {
            this->state = State::Started;
            this->startPosition = ev->globalPosition().toPoint();
            q->setHotSpot(ev->globalPosition());
            result = QGestureRecognizer::MayBeGesture;
            break;
        }
        case QEvent::MouseButtonRelease:
        {
            if (q->state() != Qt::NoGesture)
                result = QGestureRecognizer::FinishGesture;
            else
                result = QGestureRecognizer::CancelGesture;
            break;
        }
        case QEvent::MouseMove:
        {
            if (this->state == State::NoGesture)
            {
                result = QGestureRecognizer::CancelGesture;
                break;
            }

            const QPoint mousepos = ev->globalPosition().toPoint();
            const int xDistance = mousepos.x() - this->startPosition.x();
            const int yDistance = mousepos.y() - this->startPosition.y();

            if (qAbs(xDistance) > MoveThreshold || qAbs(yDistance) > MoveThreshold)
            {
                // measure the distance to check if the direction changed
                q->setSwipeAngle(QLineF(this->startPosition, mousepos).angle());
                this->startPosition = mousepos;

                result = QGestureRecognizer::TriggerGesture;
                // QTBUG-46195, small changes in direction should not cause the gesture to be canceled.
                if (this->verticalDirection == QSwipeGesture::NoDirection ||
                    qAbs(yDistance) > directionChangeThreshold)
                {
                    const QSwipeGesture::SwipeDirection vertical =
                        yDistance > 0 ? QSwipeGesture::Down : QSwipeGesture::Up;
                    if (this->verticalDirection != QSwipeGesture::NoDirection &&
                        this->verticalDirection != vertical)
                        result = QGestureRecognizer::CancelGesture;
                    this->verticalDirection = vertical;
                }
                if (this->horizontalDirection == QSwipeGesture::NoDirection ||
                    qAbs(xDistance) > directionChangeThreshold)
                {
                    const QSwipeGesture::SwipeDirection horizontal =
                        xDistance > 0 ? QSwipeGesture::Right : QSwipeGesture::Left;
                    if (this->horizontalDirection != QSwipeGesture::NoDirection &&
                        this->horizontalDirection != horizontal)
                        result = QGestureRecognizer::CancelGesture;
                    this->horizontalDirection = horizontal;
                }
            }
            else
            {
                if (q->state() != Qt::NoGesture)
                    result = QGestureRecognizer::TriggerGesture;
                else
                    result = QGestureRecognizer::MayBeGesture;
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
    this->startPosition = QPoint();
    this->state = State::NoGesture;

    QGestureRecognizer::reset(state);
}

TapGestureRecognizer::TapGestureRecognizer() : position(), pressed(false), timer() {}

QGesture *TapGestureRecognizer::create(QObject *)
{
    return new QTapGesture;
}

QGestureRecognizer::Result TapGestureRecognizer::recognize(QGesture *state, QObject *, QEvent *event)
{
    QTapGesture *q = static_cast<QTapGesture *>(state);

    const QMouseEvent *ev = static_cast<const QMouseEvent *>(event);

    QGestureRecognizer::Result result = QGestureRecognizer::CancelGesture;

    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        {
            this->position = ev->globalPosition();
            q->setHotSpot(this->position);
            timer.start();
            this->pressed = true;
            result = QGestureRecognizer::MayBeGesture;
            break;
        }
        case QEvent::MouseMove:
            break;
        case QEvent::MouseButtonRelease:
        {
            if (q->state() == Qt::NoGesture && this->pressed)
            {
                this->pressed = false;
                QPoint delta = ev->globalPosition().toPoint() - this->position.toPoint();
                if (delta.manhattanLength() <= TAPRADIUS && timer.elapsed() < TIMEOUT)
                    result = QGestureRecognizer::FinishGesture;
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
