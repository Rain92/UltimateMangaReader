#pragma once

#include <QObject>
#include <QTimer>

// Runs the cpu at full speed from a tap or a key press until a moment after it was released (see
// Device::setCpuBoost()): the work it starts, e.g. a page turn, doesn't wait for the governor to notice the load.
class CpuBoost : public QObject
{
    Q_OBJECT

public:
    explicit CpuBoost(QObject *parent = nullptr);
    ~CpuBoost() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QTimer timer;

    void start(int ms);
};
