#ifndef VIRTUALKEYBOARD_H_
#define VIRTUALKEYBOARD_H_

#include <QEvent>
#include <QMap>
#include <QTimer>
#include <QWidget>
#include <QtSvg/QSvgRenderer>
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
    VirtualKey* pPressedKey;
    int shiftKeyIndex;

    bool isShift;
    bool isCapsLock;
    bool isEnableCapsLock;
    bool isProcessCursorKeys;
    bool isPersistent;

    QSvgRenderer svgRenderer;
    QTimer repeatDelayTimer;
    QTimer repeatRateTimer;

    QString layout;
    int layoutNo;
    QVector<QString> layouts;
    QMap<QString, QString> map;

public:
    VirtualKeyboard(QWidget* parent = 0);
    ~VirtualKeyboard();

    bool persistent() const { return isPersistent; }

    void setPersistent(bool val) { isPersistent = val; }

    void setProcessCursorKeys(bool value) { isProcessCursorKeys = value; }

    bool getIsProcessCursorKeys() const { return isProcessCursorKeys; }

    int heightForWidth(int w) const;

    void setLayout(int l);
    void changeLayout();

protected:
    void paintEvent(QPaintEvent* event);
    void activateKey(VirtualKey* pKey, QEvent::Type t);
    void translateKey(QKeyEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);

    QRect getRectForKey(const VirtualKey& key) const;
    VirtualKey* findKeyByMousePos(int x, int y);
    VirtualKey* findKeyByCursorPos(int x, int y);
    void readLayouts();

private slots:
    void onCapsLock();
    void onAutoRepeat();
};

#endif /* VIRTUALKEYBOARD_H_ */
