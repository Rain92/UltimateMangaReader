#pragma once

#include <QEvent>
#include <QSvgRenderer>
#include <QTimer>
#include <QWidget>
#include <vector>

#define VIRTUAL_KEYBOARD_WIDTH 11
#define VIRTUAL_KEYBOARD_HEIGHT 5

class VirtualKeyboard : public QWidget
{
    Q_OBJECT
    struct VirtualKey
    {
        QPoint pos;
        QSize size;
        int key;
        QString sn;
        QString ss;
        QString image;
        VirtualKey(int x, int y, int w, int h, int k, const QString& s1,
                   const QString& s2, const QString& img = QString())
            : pos(x, y), size(w, h), key(k), sn(s1), ss(s2), image(img)
        {
        }
    };
    static const int gap;
    static const int width;
    static const int height;

    std::vector<VirtualKey> keys;

    VirtualKey* pLastKey;
    int shiftKeyIndex;

    bool isShift;
    bool isCapsLock;
    bool isEnableCapsLock;

    QSvgRenderer svgRenderer;
    QTimer repeatDelayTimer;
    QTimer repeatRateTimer;

public:
    VirtualKeyboard(QWidget* parent = nullptr);

    int heightForWidth(int w) const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void activateKey(VirtualKey* pKey, QEvent::Type t);
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    QRect getRectForKey(const VirtualKey& key) const;
    VirtualKey* findKeyByMousePos(int x, int y);

private:
    void onCapsLock();
    void onAutoRepeat(bool first);
};
