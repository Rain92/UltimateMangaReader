#ifndef CUSTOMGESTURERECOGNIZER_H
#define CUSTOMGESTURERECOGNIZER_H

#include <QGestureRecognizer>
#include <QPointF>
#include <QGesture>
#include <QElapsedTimer>

class SwipeGestureRecognizer : public QGestureRecognizer
{
    enum State {
        NoGesture,
        Started,
        ThreePointsReached
    };

    static const int MoveThreshold = 80;
    static const int directionChangeThreshold = MoveThreshold / 2;


    qreal velocity() const { return velocityValue; }
    void setVelocity(qreal value) { velocityValue = value; }

    QSwipeGesture::SwipeDirection horizontalDirection;
    QSwipeGesture::SwipeDirection verticalDirection;
    qreal swipeAngle;

    QPoint startPosition;
    State state;
    qreal velocityValue;
    QElapsedTimer time;

public:
  SwipeGestureRecognizer();
  QGesture *create(QObject *target);
  Result recognize(QGesture *state, QObject *watched, QEvent *event);
  void reset(QGesture *state);
};

class TapGestureRecognizer : public QGestureRecognizer
{
    QPointF position;
    bool pressed;
    QElapsedTimer timer;
    static const int TIMEOUT = 3000;
    static const int TAPRADIUS = 40;
public:
    TapGestureRecognizer();

    QGesture *create(QObject *target) override;
    QGestureRecognizer::Result recognize(QGesture *state, QObject *watched, QEvent *event) override;
    void reset(QGesture *state) override;
};


#endif // CUSTOMGESTURERECOGNIZER_H
