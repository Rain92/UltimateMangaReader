#include "mangareaderwidget.h"

#include <QPainter>

#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#include "configs.h"
#include "customgesturerecognizer.h"
#include "ui_mangareaderwidget.h"

MangaReaderWidget::MangaReaderWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::MangaReaderWidget),
      pagechanging(false),
      imgcache(),
      imgcachepaths()
{
    ui->setupUi(this);
    adjustSizes();

    setAttribute(Qt::WA_AcceptTouchEvents, true);

    ui->readerFrontLightBar->setVisible(false);
    ui->readerNavigationBar->setVisible(false);

    gotodialog = new GotoDialog(this);

#ifdef DESKTOP
    QGestureRecognizer::registerRecognizer(new TapGestureRecognizer());
#endif

    QGestureRecognizer::registerRecognizer(new SwipeGestureRecognizer());
    grabGesture(Qt::GestureType::TapGesture);
    grabGesture(Qt::GestureType::SwipeGesture);

    updateTime();
}

MangaReaderWidget::~MangaReaderWidget() { delete ui; }

void MangaReaderWidget::adjustSizes()
{
    ui->pushButtonReaderBack->setMinimumHeight(buttonsize);
    ui->pushButtonReaderClose->setMinimumHeight(buttonsize);
    ui->pushButtonReaderHome->setMinimumHeight(buttonsize);
    ui->pushButtonReaderGoto->setMinimumHeight(buttonsize);

    ui->horizontalSliderLight->setMinimumHeight(resourceiconsize);
    ui->horizontalSliderComfLight->setMinimumHeight(resourceiconsize);

    ui->labelLessLight->setMinimumSize(
        QSize(resourceiconsize, resourceiconsize));
    ui->labelLessLight->setMaximumSize(
        QSize(resourceiconsize, resourceiconsize));

    QPixmap p(":/resources/images/icons/darker.png");
    ui->labelLessLight->setPixmap(
        p.scaledToHeight(resourceiconsize, Qt::SmoothTransformation));

    ui->labelMoreLight->setMinimumSize(
        QSize(resourceiconsize, resourceiconsize));
    ui->labelMoreLight->setMaximumSize(
        QSize(resourceiconsize, resourceiconsize));

    QPixmap p2(":/resources/images/icons/lighter.png");
    ui->labelMoreLight->setPixmap(
        p2.scaledToHeight(resourceiconsize, Qt::SmoothTransformation));

    ui->labelLessComfLight->setMinimumSize(
        QSize(resourceiconsize, resourceiconsize));
    ui->labelLessComfLight->setMaximumSize(
        QSize(resourceiconsize, resourceiconsize));

    QPixmap p3(":/resources/images/icons/sun.png");
    ui->labelLessComfLight->setPixmap(
        p3.scaledToHeight(resourceiconsize, Qt::SmoothTransformation));

    ui->labelMoreComfLight->setMinimumSize(
        QSize(resourceiconsize, resourceiconsize));
    ui->labelMoreComfLight->setMaximumSize(
        QSize(resourceiconsize, resourceiconsize));

    QPixmap p4(":/resources/images/icons/moon.png");
    ui->labelMoreComfLight->setPixmap(
        p4.scaledToHeight(resourceiconsize, Qt::SmoothTransformation));

    batteryicons[0] =
        QPixmap(":/resources/images/icons/batteryfull.png")
            .scaledToHeight(batteryiconsize, Qt::SmoothTransformation);
    batteryicons[1] =
        QPixmap(":/resources/images/icons/batterycharging.png")
            .scaledToHeight(batteryiconsize, Qt::SmoothTransformation);
    batteryicons[2] =
        QPixmap(":/resources/images/icons/batteryempty.png")
            .scaledToHeight(batteryiconsize, Qt::SmoothTransformation);

    QString sliderstylesheet =
        "QSlider::groove:horizontal {       "
        "     border: 1px solid black;      "
        "     border-radius: 5px;           "
        "     height: %1px;                 "
        "     background: white;            "
        "     margin: 4px 0;                "
        "}                                  "
        "QSlider::handle:horizontal {       "
        "     background: white;            "
        "     border: 1px solid black;      "
        "     width: %3px;                  "
        "     height: %2px;                 "
        "     margin: -7px 0;               "
        "     border-radius: 9px;           "
        "}                                  "
        "                                   "
        "QSlider::handle:horizontal::focus {"
        "     background: black;            "
        "}";

    ui->horizontalSliderComfLight->setFixedHeight(frontlightsliderhandleheight +
                                                  10);
    ui->horizontalSliderLight->setFixedHeight(frontlightsliderhandleheight +
                                              10);

    ui->horizontalSliderComfLight->setStyleSheet(
        sliderstylesheet.arg(frontlightslidergrooveheight)
            .arg(frontlightsliderhandlewidth)
            .arg(frontlightsliderhandleheight));

    ui->horizontalSliderLight->setStyleSheet(
        sliderstylesheet.arg(frontlightslidergrooveheight)
            .arg(frontlightsliderhandlewidth)
            .arg(frontlightsliderhandleheight));

    ui->horizontalSliderComfLight->setInvertedAppearance(true);
}

bool MangaReaderWidget::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture)
        return gestureEvent(static_cast<QGestureEvent *>(event));
    return QWidget::event(event);
}

bool MangaReaderWidget::gestureEvent(QGestureEvent *event)
{
    if (QGesture *gesture = event->gesture(Qt::SwipeGesture))
    {
        auto swipe = static_cast<QSwipeGesture *>(gesture);
        if (swipe->state() == Qt::GestureFinished)
        {
            auto angle = swipe->swipeAngle();
            if (!pagechanging && angle > 155 && angle < 205)
            {
                pagechanging = true;
                emit advancPageClicked(true);
            }
            else if (!pagechanging && (angle > 335 || angle < 25))
            {
                pagechanging = true;
                emit advancPageClicked(false);
            }
            else if (!pagechanging &&
                     swipe->hotSpot().y() <
                         this->height() * readerbottommenuethreshold &&
                     angle > 245 && angle < 295)
            {
                showMenuBar(true);
            }
        }
    }
    else if (QGesture *gesture = event->gesture(Qt::TapGesture))
    {
        auto pos = gesture->hotSpot().toPoint();

#ifdef DESKTOP
        pos = this->mapFromGlobal(pos);
#endif

        if (gesture->state() == Qt::GestureFinished)
        {
            if (ui->readerNavigationBar->isVisible())
            {
                if (pos.y() > this->height() * readerbottommenuethreshold * 2)
                {
                    showMenuBar(false);
                }
            }
            else if (pos.y() < this->height() * readerbottommenuethreshold ||
                     pos.y() >
                         this->height() * (1.0 - readerbottommenuethreshold))
            {
                showMenuBar(true);
            }
            else if (!pagechanging)
            {
                pagechanging = true;
                if (pos.x() > this->width() * readerpreviouspagethreshold)
                {
                    emit advancPageClicked(true);
                }
                else
                {
                    emit advancPageClicked(false);
                }
            }
        }
    }

    return true;
}

void MangaReaderWidget::updateTime()
{
    QTime now = QTime::currentTime();
    ui->labelTime->setText(now.toString("hh:mm"));

    int msecsleft = (60 - now.second()) * 1000 - now.msec();
    QTimer::singleShot(msecsleft, this, SLOT(updateTime()));
}

void MangaReaderWidget::updateReaderLabels(QSharedPointer<MangaInfo> info)
{
    if (this->currentmanga.data() != info.data())
    {
        this->currentmanga.clear();
        this->currentmanga = info;
    }

    ui->labelReaderChapter->setText(
        "Chapter: " + QString::number(info->currentindex.chapter + 1) + "/" +
        QString::number(info->numchapters));
    ui->labelReaderPage->setText(
        "Page: " + QString::number(info->currentindex.page + 1) + "/" +
        QString::number(
            info->chapters.at(info->currentindex.chapter).numpages));
}

void MangaReaderWidget::on_pushButtonReaderHome_clicked()
{
    pagechanging = false;
    emit changeView(0);
}

void MangaReaderWidget::on_pushButtonReaderBack_clicked()
{
    pagechanging = false;
    emit back();
}

void MangaReaderWidget::on_pushButtonReaderClose_clicked() { emit closeApp(); }

void MangaReaderWidget::showImage(const QString &path)
{
    showMenuBar(false);

    if (path != "")
    {
        int i = imgcachepaths.indexOf(path);

        if (i != -1)
        {
            qDebug() << "Cachehit:" << i;
            ui->mangaImageContainer->setImage(*imgcache[i]);
        }
        else
        {
            qDebug() << "No cachehit.";
            addImageToCache(path);
            ui->mangaImageContainer->setImage(*imgcache[0]);
        }
    }
    pagechanging = false;
}

void MangaReaderWidget::addImageToCache(const QString &path)
{
    int i = imgcachepaths.indexOf(path);
    if (i != -1)
    {
        imgcachepaths.move(i, 0);
        imgcache.move(i, 0);
    }
    else
    {
        imgcachepaths.insert(0, path);
        imgcache.insert(0, new QPixmap(path));

        if (imgcache.count() > imagecachesize)
        {
            imgcachepaths.removeLast();
            delete imgcache.last();
            imgcache.removeLast();
        }
    }
}

void MangaReaderWidget::setFrontLightPanelState(int lightmin, int lightmax,
                                                int light, int comflightmin,
                                                int comflightmax, int comflight)
{
    ui->horizontalSliderLight->setMinimum(lightmin);
    ui->horizontalSliderLight->setMaximum(lightmax);
    ui->horizontalSliderLight->setValue(light);

    if (comflightmin != comflightmax)
    {
        ui->horizontalSliderComfLight->setMinimum(comflightmin);
        ui->horizontalSliderComfLight->setMaximum(comflightmax);
        ui->horizontalSliderComfLight->setValue(comflight);
    }
    else
    {
        ui->horizontalSliderComfLight->hide();
        ui->labelLessComfLight->hide();
        ui->labelMoreComfLight->hide();
    }
}

void MangaReaderWidget::setFrontLightPanelState(int light, int comflight)
{
    ui->horizontalSliderLight->setValue(light);
    ui->horizontalSliderComfLight->setValue(comflight);
}

void MangaReaderWidget::on_horizontalSliderLight_valueChanged(int value)
{
    emit frontlightchanged(value, ui->horizontalSliderComfLight->value());
}

void MangaReaderWidget::on_horizontalSliderComfLight_valueChanged(int value)
{
    emit frontlightchanged(ui->horizontalSliderLight->value(), value);
}

void MangaReaderWidget::on_pushButtonReaderGoto_clicked()
{
    gotodialog->setup(
        currentmanga->numchapters,
        currentmanga->chapters[currentmanga->currentindex.chapter].numpages,
        currentmanga->currentindex);

    emit enableVirtualKeyboard(false);
    if (gotodialog->exec() == QDialog::Accepted &&
        !gotodialog->selectedindex.illegal)
    {
        showMenuBar(false);

        emit gotoIndex(gotodialog->selectedindex);
    }
    emit enableVirtualKeyboard(true);
}

void MangaReaderWidget::setBatteryIcon()
{
    QPair<int, bool> batterystate = getBatteryState();
    int bat = batterystate.first;
    bool charging = batterystate.second;

    if (bat >= 98)
        ui->labelBattery->setPixmap(batteryicons[0]);
    else if (charging)
        ui->labelBattery->setPixmap(batteryicons[1]);
    else
    {
        batteryicons[3] = QPixmap(":/resources/images/icons/batteryempty.png");

        QPainter painter(&batteryicons[3]);
        QBrush brush(Qt::black);

        if (bat > 90)
        {
            int w = (bat - 90) / 2;
            painter.fillRect(7 + (5 - w), 12, w, 8, brush);
        }

        int w = qMin(45, bat / 2);
        painter.fillRect(12 + (45 - w), 6, w, 20, brush);

        painter.end();
        ui->labelBattery->setPixmap(batteryicons[3].scaledToHeight(
            batteryiconsize, Qt::SmoothTransformation));
    }
}

QPair<int, bool> MangaReaderWidget::getBatteryState()
{
#ifdef KOBO
    return QPair<int, bool>(KoboPlatformFunctions::getBatteryLevel(),
                            KoboPlatformFunctions::isBatteryCharging());
#endif

    return QPair<int, bool>(100, false);
}

void MangaReaderWidget::showMenuBar(bool show)
{
    if (!show && ui->readerNavigationBar->isVisible())
    {
        ui->readerFrontLightBar->setVisible(false);
        ui->readerNavigationBar->setVisible(false);
    }
    else if (show && !ui->readerNavigationBar->isVisible())
    {
        setBatteryIcon();
        ui->readerNavigationBar->setVisible(true);
        ui->readerFrontLightBar->setVisible(true);
    }
}
