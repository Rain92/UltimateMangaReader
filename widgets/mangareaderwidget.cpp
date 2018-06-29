#include "mangareaderwidget.h"
#include "ui_mangareaderwidget.h"
#include "configs.h"
#include <QPainter>



MangaReaderWidget::MangaReaderWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MangaReaderWidget),
    pagechanging(false),
    imgcache(),
    imgcachepaths()
{
    ui->setupUi(this);
    adjustSizes();

    ui->readerFrontLightBar->setVisible(false);
    ui->readerNavigationBar->setVisible(false);

    gotodialog = new GotoDialog(this);

    updateTime();
}

MangaReaderWidget::~MangaReaderWidget()
{
    delete ui;
}


void  MangaReaderWidget::adjustSizes()
{
    ui->pushButtonReaderBack->setMinimumHeight(buttonsize);
    ui->pushButtonReaderClose->setMinimumHeight(buttonsize);
    ui->pushButtonReaderHome->setMinimumHeight(buttonsize);
    ui->pushButtonReaderGoto->setMinimumHeight(buttonsize);


    ui->horizontalSliderLight->setMinimumHeight(resourceiconsize);
    ui->horizontalSliderComfLight->setMinimumHeight(resourceiconsize);

    ui->labelLessLight->setMinimumSize(QSize(resourceiconsize, resourceiconsize));
    ui->labelLessLight->setMaximumSize(QSize(resourceiconsize, resourceiconsize));

    QPixmap p(":/resources/images/icons/darker.png");
    ui->labelLessLight->setPixmap(p.scaledToHeight(resourceiconsize, Qt::SmoothTransformation));

    ui->labelMoreLight->setMinimumSize(QSize(resourceiconsize, resourceiconsize));
    ui->labelMoreLight->setMaximumSize(QSize(resourceiconsize, resourceiconsize));

    QPixmap p2(":/resources/images/icons/lighter.png");
    ui->labelMoreLight->setPixmap(p2.scaledToHeight(resourceiconsize, Qt::SmoothTransformation));


    ui->labelLessComfLight->setMinimumSize(QSize(resourceiconsize, resourceiconsize));
    ui->labelLessComfLight->setMaximumSize(QSize(resourceiconsize, resourceiconsize));

    QPixmap p3(":/resources/images/icons/sun.png");
    ui->labelLessComfLight->setPixmap(p3.scaledToHeight(resourceiconsize, Qt::SmoothTransformation));

    ui->labelMoreComfLight->setMinimumSize(QSize(resourceiconsize, resourceiconsize));
    ui->labelMoreComfLight->setMaximumSize(QSize(resourceiconsize, resourceiconsize));

    QPixmap p4(":/resources/images/icons/moon.png");
    ui->labelMoreComfLight->setPixmap(p4.scaledToHeight(resourceiconsize, Qt::SmoothTransformation));


    batteryicons[0] = QPixmap(":/resources/images/icons/batteryfull.png").scaledToHeight(batteryiconsize, Qt::SmoothTransformation);
    batteryicons[1] = QPixmap(":/resources/images/icons/batterycharging.png").scaledToHeight(batteryiconsize, Qt::SmoothTransformation);
    batteryicons[2] = QPixmap(":/resources/images/icons/batteryempty.png").scaledToHeight(batteryiconsize, Qt::SmoothTransformation);



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

    ui->horizontalSliderComfLight->setFixedHeight(frontlightsliderhandleheight + 10);
    ui->horizontalSliderLight->setFixedHeight(frontlightsliderhandleheight + 10);

    ui->horizontalSliderComfLight->setStyleSheet(sliderstylesheet.arg(frontlightslidergrooveheight).arg(frontlightsliderhandlewidth).arg(
                                                     frontlightsliderhandleheight));

    ui->horizontalSliderLight->setStyleSheet(sliderstylesheet.arg(frontlightslidergrooveheight).arg(frontlightsliderhandlewidth).arg(
                                                 frontlightsliderhandleheight));

    ui->horizontalSliderComfLight->setInvertedAppearance(true);

#ifndef WINDOWS
    GesturesController *g = new GesturesController(ui->mangaImageContainer);
    QObject::connect(g, SIGNAL(sigGesture(QPoint, GesturesController::GestureType)), this, SLOT(gestureInput(QPoint, GesturesController::GestureType)));
#endif
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

    ui->labelReaderChapter->setText("Chapter: " + QString::number(info->currentindex.chapter + 1) +
                                    "/" + QString::number(info->numchapters));
    ui->labelReaderPage->setText("Page: " + QString::number(info->currentindex.page + 1) + "/" +
                                 QString::number(info->chapters.at(info->currentindex.chapter).numpages));
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

void MangaReaderWidget::on_pushButtonReaderClose_clicked()
{
    emit closeApp();
}

void MangaReaderWidget::showImage(const QString &path)
{
    showMenuBar(false);

    if (path != "")
    {
        int i = imgcachepaths.indexOf(path);

        if (i != -1)
        {
            qDebug() << "cachehit!" << i;
            ui->mangaImageContainer->setImage(*imgcache[i]);
        }
        else
        {
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

void MangaReaderWidget::on_mangaImageContainer_clicked(QPoint pos)
{
#ifdef WINDOWS
    if (ui->readerNavigationBar->isVisible())
    {
        if (pos.y() > this->height() * readerbottommenuethreshold * 2)
        {
            showMenuBar(false);
        }
    }
    else if (pos.y() < this->height() * readerbottommenuethreshold || pos.y() > this->height() * (1.0 - readerbottommenuethreshold))
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
#else
    Q_UNUSED(pos);
#endif
}

void MangaReaderWidget::setFrontLightPanelState(int lightmin, int lightmax, int light, int comflightmin, int comflightmax, int comflight)
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
    gotodialog->setup(currentmanga->numchapters, currentmanga->chapters[currentmanga->currentindex.chapter].numpages, currentmanga->currentindex);

    emit enableVirtualKeyboard(false);
    if (gotodialog->exec() == QDialog::Accepted && !gotodialog->selectedindex.illegal)
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
        ui->labelBattery->setPixmap(batteryicons[3].scaledToHeight(batteryiconsize, Qt::SmoothTransformation));
    }
}

QPair<int, bool> MangaReaderWidget::getBatteryState()
{
#ifndef WINDOWS
    return QPair<int, bool>(Platform::get()->getBatteryLevel(), Platform::get()->isBatteryCharging());
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

#ifndef WINDOWS
void MangaReaderWidget::gestureInput(QPoint pos, GesturesController::GestureType gesture)
{
    if (gesture == GesturesController::gtSwipeRL && !pagechanging)
    {
        pagechanging = true;
        emit advancPageClicked(true);
    }
    else if (gesture == GesturesController::gtSwipeLR && !pagechanging)
    {
        pagechanging = true;
        emit advancPageClicked(false);
    }
    else if (gesture == GesturesController::gtSwipeTB && pos.y() < this->height() * readerbottommenuethreshold)
    {
        showMenuBar(true);
    }
    else if (gesture == GesturesController::gtTapShort)
    {
        if (ui->readerNavigationBar->isVisible())
        {
            if (pos.y() > this->height() * readerbottommenuethreshold * 2)
            {
                showMenuBar(false);
            }
        }
        else if (pos.y() < this->height() * readerbottommenuethreshold || pos.y() > this->height() * (1.0 - readerbottommenuethreshold))
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
#endif
