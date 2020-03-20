#include "VirtualKeyboard.h"

#include <QApplication>
#include <QDebug>
#include <QPaintEvent>
#include <QPainter>
#include <QTimer>

const int VirtualKeyboard::gap = 3;
const int VirtualKeyboard::width = VIRTUAL_KEYBOARD_WIDTH;
const int VirtualKeyboard::height = VIRTUAL_KEYBOARD_HEIGHT;

static QRect ir(const QRect& r, int size)
{
    return QRect(r.x() + size, r.y() + size, r.width() - 2 * size,
                 r.height() - 2 * size);
}

VirtualKeyboard::VirtualKeyboard(QWidget* parent)
    : QWidget(parent),
      pLastKey(NULL),
      shiftKeyIndex(0),
      isShift(false),
      isCapsLock(false),
      isEnableCapsLock(false),
      isProcessCursorKeys(true),
      isPersistent(false),
      svgRenderer(QString(":/resources/images/keyboardicons.svg")),
      layoutNo(0)
{
    setFocusPolicy(Qt::NoFocus);

    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

    keys.reserve(width * height);

    keys.push_back(VirtualKey(0, 0, 1, 1, Qt::Key_1, "1", "!"));
    keys.push_back(VirtualKey(1, 0, 1, 1, Qt::Key_2, "2", "@"));
    keys.push_back(VirtualKey(2, 0, 1, 1, Qt::Key_3, "3", "#"));
    keys.push_back(VirtualKey(3, 0, 1, 1, Qt::Key_4, "4", "$"));
    keys.push_back(VirtualKey(4, 0, 1, 1, Qt::Key_5, "5", "%"));
    keys.push_back(VirtualKey(5, 0, 1, 1, Qt::Key_6, "6", "^"));
    keys.push_back(VirtualKey(6, 0, 1, 1, Qt::Key_7, "7", "&"));
    keys.push_back(VirtualKey(7, 0, 1, 1, Qt::Key_8, "8", "*"));
    keys.push_back(VirtualKey(8, 0, 1, 1, Qt::Key_9, "9", "("));
    keys.push_back(VirtualKey(9, 0, 1, 1, Qt::Key_0, "0", ")"));
    keys.push_back(VirtualKey(10, 0, 1, 1, Qt::Key_Close, "\0", "\0", "close"));
    int keyCloseIndex = keys.size() - 1;

    keys.push_back(VirtualKey(0, 1, 1, 1, Qt::Key_Q, "q", "Q"));
    keys.push_back(VirtualKey(1, 1, 1, 1, Qt::Key_W, "w", "W"));
    keys.push_back(VirtualKey(2, 1, 1, 1, Qt::Key_E, "e", "E"));
    keys.push_back(VirtualKey(3, 1, 1, 1, Qt::Key_R, "r", "R"));
    keys.push_back(VirtualKey(4, 1, 1, 1, Qt::Key_T, "t", "T"));
    keys.push_back(VirtualKey(5, 1, 1, 1, Qt::Key_Y, "y", "Y"));
    keys.push_back(VirtualKey(6, 1, 1, 1, Qt::Key_U, "u", "U"));
    keys.push_back(VirtualKey(7, 1, 1, 1, Qt::Key_I, "i", "I"));
    keys.push_back(VirtualKey(8, 1, 1, 1, Qt::Key_O, "o", "O"));
    keys.push_back(VirtualKey(9, 1, 1, 1, Qt::Key_P, "p", "P"));
    keys.push_back(
        VirtualKey(10, 1, 1, 1, Qt::Key_Backspace, "", "", "delete"));

    keys.push_back(VirtualKey(0, 2, 1, 1, Qt::Key_A, "a", "A"));
    keys.push_back(VirtualKey(1, 2, 1, 1, Qt::Key_S, "s", "S"));
    keys.push_back(VirtualKey(2, 2, 1, 1, Qt::Key_D, "d", "D"));
    keys.push_back(VirtualKey(3, 2, 1, 1, Qt::Key_F, "f", "F"));
    keys.push_back(VirtualKey(4, 2, 1, 1, Qt::Key_G, "g", "G"));
    keys.push_back(VirtualKey(5, 2, 1, 1, Qt::Key_H, "h", "H"));
    keys.push_back(VirtualKey(6, 2, 1, 1, Qt::Key_J, "j", "J"));
    keys.push_back(VirtualKey(7, 2, 1, 1, Qt::Key_K, "k", "K"));
    keys.push_back(VirtualKey(8, 2, 1, 1, Qt::Key_L, "l", "L"));
    keys.push_back(VirtualKey(9, 2, 1, 1, Qt::Key_Semicolon, ":", ";"));
    keys.push_back(VirtualKey(10, 2, 1, 1, Qt::Key_Return, "", "", "enter"));

    keys.push_back(VirtualKey(0, 3, 1, 1, Qt::Key_Z, "z", "Z"));
    keys.push_back(VirtualKey(1, 3, 1, 1, Qt::Key_X, "x", "X"));
    keys.push_back(VirtualKey(2, 3, 1, 1, Qt::Key_C, "c", "C"));
    keys.push_back(VirtualKey(3, 3, 1, 1, Qt::Key_V, "v", "V"));
    keys.push_back(VirtualKey(4, 3, 1, 1, Qt::Key_B, "b", "B"));
    keys.push_back(VirtualKey(5, 3, 1, 1, Qt::Key_N, "n", "N"));
    keys.push_back(VirtualKey(6, 3, 1, 1, Qt::Key_M, "m", "M"));
    keys.push_back(VirtualKey(7, 3, 1, 1, Qt::Key_Apostrophe, "'", "\""));
    keys.push_back(VirtualKey(8, 3, 1, 1, Qt::Key_Comma, ".", ","));
    keys.push_back(VirtualKey(9, 3, 1, 1, Qt::Key_Up, "", "", "up"));
    keys.push_back(VirtualKey(10, 3, 1, 1, Qt::Key_Plus, "-", "+"));

    keys.push_back(VirtualKey(0, 4, 1, 1, Qt::Key_Shift, "", "", "shift0"));
    shiftKeyIndex = keys.size() - 1;

    keys.push_back(
        VirtualKey(1, 4, 1, 1, Qt::Key_Mode_switch, "", "", "language"));
    keys.push_back(VirtualKey(2, 4, 1, 1, Qt::Key_BraceLeft, "[", "{"));
    keys.push_back(VirtualKey(3, 4, 1, 1, Qt::Key_BraceRight, "]", "}"));
    keys.push_back(VirtualKey(4, 4, 2, 1, Qt::Key_Space, " ", " "));
    keys.push_back(VirtualKey(6, 4, 1, 1, Qt::Key_Question, "?", "/"));
    keys.push_back(VirtualKey(7, 4, 1, 1, Qt::Key_Underscore, "_", "="));
    keys.push_back(VirtualKey(8, 4, 1, 1, Qt::Key_Left, "", "", "left"));
    keys.push_back(VirtualKey(9, 4, 1, 1, Qt::Key_Down, "", "", "down"));
    keys.push_back(VirtualKey(10, 4, 1, 1, Qt::Key_Right, "", "", "right"));

    pLastKey = &keys[keyCloseIndex];

    readLayouts();
    //    if ( g_pConfig )
    //        setLayout(g_pConfig->readInt("kbd_layout", 0));

    repeatDelayTimer.setInterval(500);
    repeatDelayTimer.setSingleShot(true);
    repeatRateTimer.setInterval(150);

    QObject::connect(&repeatDelayTimer, SIGNAL(timeout()), this,
                     SLOT(onAutoRepeat()));
    QObject::connect(&repeatRateTimer, SIGNAL(timeout()), this,
                     SLOT(onAutoRepeat()));
}

VirtualKeyboard::~VirtualKeyboard()
{
    //    if ( g_pConfig )
    //        g_pConfig->writeInt("kbd_layout", layoutNo);
}

int VirtualKeyboard::heightForWidth(int w) const { return w * 5 / 11; }

void VirtualKeyboard::setLayout(int l)
{
    if (l >= 0 && l < layouts.size())
    {
        map.clear();

        layoutNo = l;

        QStringList list(layouts[layoutNo].split(" "));

        if (list.empty())
            return;

        layout = list[0];

        for (int n = 1; n < list.size(); ++n)
        {
            if (2 == list[n].length())
                map[QString(list[n][0])] = QString(list[n][1]);
        }

        update();
    }
}

void VirtualKeyboard::changeLayout()
{
    setLayout((layoutNo + 1) % layouts.size());
}

void VirtualKeyboard::paintEvent(QPaintEvent* event)
{
    QPainter p(this);

    p.setBrush(QBrush(palette().color(QPalette::Window)));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    p.setBrush(Qt::NoBrush);

    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPen pen1(Qt::SolidLine);
    pen1.setWidth(3);
    QPen pen2(Qt::SolidLine);
    pen2.setWidth(0);

    int fontSize = 0;

    for (std::vector<VirtualKey>::const_iterator i = keys.begin();
         i != keys.end(); ++i)
    {
        QRect r(getRectForKey(*i));

        if (r.intersects(event->rect()))
        {
            p.setPen(&*i == pLastKey ? pen1 : pen2);

            QString s(isShift ? i->ss : i->sn);
            QMap<QString, QString>::ConstIterator it(map.find(s));
            if (it != map.end())
                s = it.value();

            if (!fontSize)
            {
                fontSize = r.height() * 8 / 10;
                QFont f(p.font());
                f.setPixelSize(fontSize);
                p.setFont(f);
            }

            if (!i->image.isEmpty())
            {
                QRect ir(r);  // image rect for aspect ratio

                ir.setWidth(ir.width() * 2 / 3);
                ir.setHeight(ir.height() * 2 / 3);

                QRectF bounds(svgRenderer.boundsOnElement(i->image));
                if (bounds.width() / bounds.height() > 1.1)
                    ir.setHeight(ir.height() * bounds.height() /
                                 bounds.width());
                else if (bounds.height() / bounds.width() > 1.1)
                    ir.setWidth(ir.width() * bounds.width() / bounds.height());

                ir.translate((r.width() + 1 - ir.width()) / 2,
                             (r.height() + 1 - ir.height()) / 2);

                svgRenderer.render(&p, i->image, ir);
            }
            else
            {
                p.drawText(r, Qt::AlignVCenter | Qt::AlignHCenter, s);
            }

            //            p.drawRoundRect(r, 40, 40);
            p.drawRect(r);
        }
    }
}

void VirtualKeyboard::activateKey(VirtualKey* pKey, QEvent::Type t)
{
    if (pKey)
    {
        bool needFullUpdate = false;
        const VirtualKey* pOldLastKey = pLastKey;

        if (QEvent::KeyPress == t)
            pLastKey = pKey;

        if (Qt::Key_Shift == pKey->key)
        {
            if (QEvent::KeyPress == t)
            {
                if (isShift)
                {
                    if (isCapsLock)
                    {
                        // shift and caps lock:
                        // just turn shift and caps lock off
                        isShift = false;
                        isCapsLock = false;
                        pKey->image = "shift0";
                        needFullUpdate = true;
                    }
                    else
                    {
                        // shift and not caps lock:
                        // check isEnableCapsLock
                        if (isEnableCapsLock)
                        {
                            // caps lock is enabled:
                            // turn on caps lock
                            isCapsLock = true;
                            pKey->image = "shift2";
                        }
                        else
                        {
                            // caps lock is not enabled:
                            // turn off shift
                            isShift = false;
                            pKey->image = "shift0";
                            needFullUpdate = true;
                        }
                    }
                }
                else
                {
                    // turn on shift
                    isShift = true;
                    isCapsLock = false;
                    isEnableCapsLock = true;
                    pKey->image = "shift1";
                    needFullUpdate = true;
                    QTimer::singleShot(1000, this, SLOT(onCapsLock()));
                }

                if (needFullUpdate)
                {
                    update();
                }
                else
                {
                    if (pLastKey != pOldLastKey)
                    {
                        if (pOldLastKey)
                            update(ir(getRectForKey(*pOldLastKey), -2));
                    }
                    if (pLastKey)
                        update(ir(getRectForKey(*pLastKey), -2));
                }
            }
        }
        else if (Qt::Key_Mode_switch == pKey->key)
        {
            if (QEvent::KeyPress == t)
            {
                changeLayout();
            }
        }
        else if (Qt::Key_Close == pKey->key)
        {
            if (QEvent::KeyRelease == t)
            {
                if (pLastKey != pOldLastKey)
                {
                    if (pOldLastKey)
                        update(ir(getRectForKey(*pOldLastKey), -2));
                    if (pLastKey)
                        update(ir(getRectForKey(*pLastKey), -2));
                }
                hide();
            }
        }
        else
        {
            QString s(isShift ? pKey->ss : pKey->sn);
            QMap<QString, QString>::ConstIterator i(map.find(s));
            if (i != map.end())
                s = i.value();
            Qt::KeyboardModifiers mod =
                isShift ? Qt::ShiftModifier : Qt::NoModifier;
            QWidget* w = qApp->focusWidget();
            if (w != NULL)
                QCoreApplication::postEvent(
                    w, new QKeyEvent(t, pKey->key, mod, s));

            QRect r(getRectForKey(*pKey));
            r.setX(r.x() - gap);
            r.setY(r.y() - gap);
            r.setWidth(r.width() + 2 * gap);
            r.setHeight(r.height() + 2 * gap);

            if (pKey->key != Qt::Key_Up && pKey->key != Qt::Key_Down &&
                pKey->key != Qt::Key_Left && pKey->key != Qt::Key_Right)
            {
                if (isShift && !isCapsLock)
                {
                    needFullUpdate = true;
                    isShift = false;
                    keys[shiftKeyIndex].image = "shift0";
                }
            }

            if (needFullUpdate)
            {
                update();
            }
            else
            {
                if (pLastKey != pOldLastKey)
                {
                    if (pOldLastKey)
                        update(ir(getRectForKey(*pOldLastKey), -2));
                    if (pLastKey)
                        update(ir(getRectForKey(*pLastKey), -2));
                }
            }
        }
    }
}

void VirtualKeyboard::translateKey(QKeyEvent* event)
{
    for (std::vector<VirtualKey>::iterator i = keys.begin(); i != keys.end();
         ++i)
    {
        if (event->key() == i->key)
        {
            activateKey(&*i, event->type());
        }
    }
}

void VirtualKeyboard::mousePressEvent(QMouseEvent* event)
{
    VirtualKey* pKey = findKeyByMousePos(event->x(), event->y());
    if (pKey)
    {
        activateKey(pKey, QEvent::KeyPress);
        repeatDelayTimer.start();
    }
}

void VirtualKeyboard::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
    if (pLastKey)
    {
        activateKey(pLastKey, QEvent::KeyRelease);
        if (repeatDelayTimer.isActive())
            repeatDelayTimer.stop();
        if (repeatRateTimer.isActive())
            repeatRateTimer.stop();
    }
}

QRect VirtualKeyboard::getRectForKey(
    const VirtualKeyboard::VirtualKey& key) const
{
    QRect r(this->rect());
    int cw = (r.width() - (width + 1) * gap) / width;
    int ch = (r.height() - (height + 1) * gap) / height;

    int x = key.pos.x() * cw + (key.pos.x() + 1) * gap;
    int y = key.pos.y() * ch + (key.pos.y() + 1) * gap;

    int w = key.size.width() * cw + (key.size.width() - 1) * gap;
    int h = key.size.height() * ch + (key.size.height() - 1) * gap;

    return QRect(x, y, w, h);
}

VirtualKeyboard::VirtualKey* VirtualKeyboard::findKeyByMousePos(int x, int y)
{
    for (std::vector<VirtualKey>::iterator i = keys.begin(); i != keys.end();
         ++i)
    {
        QRect r(getRectForKey(*i));
        if (r.contains(x, y))
            return &*i;
    }
    return NULL;
}

VirtualKeyboard::VirtualKey* VirtualKeyboard::findKeyByCursorPos(int x, int y)
{
    for (std::vector<VirtualKey>::iterator i = keys.begin(); i != keys.end();
         ++i)
    {
        if (x >= i->pos.x() && x < i->pos.x() + i->size.width() &&
            y >= i->pos.y() && y < i->pos.y() + i->size.height())
            return &*i;
    }
    return NULL;
}

void VirtualKeyboard::readLayouts()
{
    //    Factory<LanguageDescriptor>& ldf =
    //    Singleton<Factory<LanguageDescriptor> >::instance(); if (
    //    !ldf.isEmpty() )
    //    {
    //        int lang = getLangIndex();
    //        QFile file(ldf.at(lang).getKbdFileName());
    //        if (file.open(QIODevice::ReadOnly))
    //        {
    //            QTextStream in(&file);
    //            in.setCodec("UTF-8");
    //            while (!in.atEnd())
    //            {
    //                QString str(in.readLine().trimmed());
    //                if ( !str.isEmpty() )
    //                    layouts.push_back(str);
    //            }
    //        }
    //    }
    //    if ( layouts.isEmpty() )
    //    {
    layouts.push_back("EN");
    //    }
}

void VirtualKeyboard::onCapsLock() { isEnableCapsLock = false; }

void VirtualKeyboard::onAutoRepeat()
{
    if (pLastKey)
    {
        activateKey(pLastKey, QEvent::KeyPress);
        if (sender() == &repeatDelayTimer)
            repeatRateTimer.start();
    }
    else
    {
        if (repeatDelayTimer.isActive())
            repeatDelayTimer.stop();
        if (repeatRateTimer.isActive())
            repeatRateTimer.stop();
    }
}
