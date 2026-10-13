#pragma once

#include <QElapsedTimer>
#include <QGesture>
#include <QGestureRecognizer>
#include <QPointF>

// Swipes and taps from mouse events (the touch screens deliver synthesized mouse events).
class SwipeGestureRecognizer : public QGestureRecognizer
{
    enum State
    {
        NoGesture,
        Started
    };

    static const int MoveThreshold = 50;
    static const int directionChangeThreshold = MoveThreshold / 2;

    QSwipeGesture::SwipeDirection horizontalDirection;
    QSwipeGesture::SwipeDirection verticalDirection;
    QPoint startPosition;
    State state;

public:
    SwipeGestureRecognizer();
    QGesture *create(QObject *) override;
    Result recognize(QGesture *state, QObject *watched, QEvent *event) override;
    void reset(QGesture *state) override;
};

class TapGestureRecognizer : public QGestureRecognizer
{
    QPointF position;
    bool pressed;
    QElapsedTimer timer;
    static const int TIMEOUT = 3000;
    static const int TAPRADIUS = 20;

public:
    TapGestureRecognizer();

    QGesture *create(QObject *) override;
    QGestureRecognizer::Result recognize(QGesture *state, QObject *watched, QEvent *event) override;
    void reset(QGesture *state) override;
};
