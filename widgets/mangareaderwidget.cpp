#include "mangareaderwidget.h"

#include <QPainter>

#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#include "customgesturerecognizer.h"
#include "defines.h"
#include "ui_mangareaderwidget.h"

MangaReaderWidget::MangaReaderWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::MangaReaderWidget),
      pagechanging(false),
      imgcache()
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

    QPixmap p3(":/resources/images/icons/moon.png");
    ui->labelLessComfLight->setPixmap(
        p3.scaledToHeight(resourceiconsize, Qt::SmoothTransformation));

    ui->labelMoreComfLight->setMinimumSize(
        QSize(resourceiconsize, resourceiconsize));
    ui->labelMoreComfLight->setMaximumSize(
        QSize(resourceiconsize, resourceiconsize));

    QPixmap p4(":/resources/images/icons/sun.png");
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
        if (gesture->state() != Qt::GestureFinished || pagechanging)
            return true;

        auto swipe = static_cast<QSwipeGesture *>(gesture);

        auto pos = this->mapFromGlobal(gesture->hotSpot().toPoint());
        auto angle = swipe->swipeAngle();

        if (ui->readerNavigationBar->isVisible())
        {
            if (pos.y() > this->height() * readerbottommenuethreshold * 2)
                showMenuBar(false);
        }
        else if (angle > 155 && angle < 205)
        {
            pagechanging = true;
            emit advancPageClicked(Forward);
        }
        else if (angle > 335 || angle < 25)
        {
            pagechanging = true;
            emit advancPageClicked(Backward);
        }
        else if (swipe->hotSpot().y() <
                     this->height() * readerbottommenuethreshold &&
                 angle > 245 && angle < 295)
        {
            showMenuBar(true);
        }
    }
    else if (QGesture *gesture = event->gesture(Qt::TapGesture))
    {
        auto pos = this->mapFromGlobal(gesture->hotSpot().toPoint());

        if (gesture->state() != Qt::GestureFinished || pagechanging)
            return true;

        if (ui->readerNavigationBar->isVisible())
        {
            if (pos.y() > this->height() * readerbottommenuethreshold * 2)
                showMenuBar(false);
        }
        else if (pos.y() < this->height() * readerbottommenuethreshold ||
                 pos.y() > this->height() * (1.0 - readerbottommenuethreshold))
        {
            showMenuBar(true);
        }
        else
        {
            pagechanging = true;
            PageTurnDirection direction =
                pos.x() > this->width() * readerpreviouspagethreshold
                    ? Forward
                    : Backward;
            emit advancPageClicked(direction);
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

void MangaReaderWidget::updateReaderLabels(int chapter, int page,
                                           int numChapters, int numPages)
{
    ui->labelReaderChapter->setText("Chapter: " + QString::number(chapter + 1) +
                                    "/" + QString::number(numChapters));
    ui->labelReaderPage->setText("Page: " + QString::number(page + 1) + "/" +
                                 QString::number(numPages));
}

void MangaReaderWidget::on_pushButtonReaderHome_clicked()
{
    pagechanging = false;
    emit changeView(HomeTab);
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
        int i = searchCache(path);

        if (i != -1)
        {
            qDebug() << "Cachehit:" << i;
            ui->mangaImageContainer->setImage(imgcache[i].first);
        }
        else
        {
            //            qDebug() << "No cachehit.";
            addImageToCache(path);
            i = searchCache(path);
            ui->mangaImageContainer->setImage(imgcache[i].first);
        }
    }

    pagechanging = false;

    //    QTimer::singleShot(50, [this]() { pagechanging = false; });
}

void MangaReaderWidget::addImageToCache(const QString &path)
{
    int i = searchCache(path);
    if (i != -1)
    {
        imgcache.move(i, 0);
    }
    else
    {
        imgcache.insert(0, {QSharedPointer<QPixmap>(new QPixmap(path)), path});

        if (imgcache.count() > imagecachesize)
            imgcache.removeLast();
    }
}

int MangaReaderWidget::searchCache(const QString &path) const
{
    for (int i = 0; i < imgcache.size(); i++)
        if (imgcache[i].second == path)
            return i;

    return -1;
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
    // TODO
    //    gotodialog->setup(
    //        currentmanga->numChapters,
    //        currentmanga->chapters[currentmanga->currentIndex.chapter].numPages,
    //        currentmanga->currentIndex);

    //    if (gotodialog->exec() == QDialog::Accepted &&
    //        !gotodialog->selectedindex.illegal)
    //    {
    //        showMenuBar(false);

    //        emit gotoIndex(gotodialog->selectedindex);
    //    }
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
