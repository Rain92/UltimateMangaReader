#include "mangareaderwidget.h"

#include "ui_mangareaderwidget.h"

#ifdef KOBO
#include "koboplatformfunctions.h"
#endif

MangaReaderWidget::MangaReaderWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::MangaReaderWidget), imgcache(), settings(nullptr)
{
    ui->setupUi(this);
    adjustUI();

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
}

MangaReaderWidget::~MangaReaderWidget()
{
    delete ui;
}

void MangaReaderWidget::adjustUI()
{
    ui->pushButtonReaderBack->setProperty("type", "borderless");
    ui->pushButtonReaderFavorites->setProperty("type", "borderless");
    ui->pushButtonReaderHome->setProperty("type", "borderless");

    ui->pushButtonReaderBack->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonReaderFavorites->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonReaderHome->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonReaderGoto->setFixedHeight(SIZES.buttonSize);

    ui->horizontalSliderLight->setFixedHeight(SIZES.resourceIconSize);
    ui->horizontalSliderComfLight->setFixedHeight(SIZES.resourceIconSize);

    ui->labelTime->setStyleSheet("font-size: 13pt");

    ui->labelLessLight->setFixedSize(QSize(SIZES.resourceIconSize, SIZES.resourceIconSize));
    ui->labelMoreLight->setFixedSize(QSize(SIZES.resourceIconSize, SIZES.resourceIconSize));
    ui->labelLessComfLight->setFixedSize(QSize(SIZES.resourceIconSize, SIZES.resourceIconSize));
    ui->labelMoreComfLight->setFixedSize(QSize(SIZES.resourceIconSize, SIZES.resourceIconSize));

    ui->labelLessLight->setScaledContents(true);
    ui->labelMoreLight->setScaledContents(true);
    ui->labelLessComfLight->setScaledContents(true);
    ui->labelMoreComfLight->setScaledContents(true);

    ui->labelLessLight->setPixmap(QPixmap(":/images/icons/darker.png"));
    ui->labelMoreLight->setPixmap(QPixmap(":/images/icons/lighter.png"));
    ui->labelLessComfLight->setPixmap(QPixmap(":/images/icons/moon.png"));
    ui->labelMoreComfLight->setPixmap(QPixmap(":/images/icons/sun.png"));

    ui->horizontalSliderComfLight->setFixedHeight(SIZES.frontlightSliderHandleHeight);
    ui->horizontalSliderLight->setFixedHeight(SIZES.frontlightSliderHandleHeight);

    ui->horizontalSliderComfLight->setInvertedAppearance(true);
}

bool MangaReaderWidget::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture)
        return gestureEvent(static_cast<QGestureEvent *>(event));
    else if (event->type() == QEvent::KeyPress)
        return buttonPressEvent(static_cast<QKeyEvent *>(event));
    return QWidget::event(event);
}

bool MangaReaderWidget::buttonPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_PageUp)
        emit advancPageClicked(conditionalReverse(Forward, settings && settings->reverseButtonDirection));
    else if (event->key() == Qt::Key_PageDown)
        emit advancPageClicked(conditionalReverse(Forward, settings && settings->reverseButtonDirection));
    else
        return false;

    return true;
}

bool MangaReaderWidget::gestureEvent(QGestureEvent *event)
{
    if (QGesture *gesture = event->gesture(Qt::SwipeGesture))
    {
        if (gesture->state() != Qt::GestureFinished)
            return true;

        auto swipe = static_cast<QSwipeGesture *>(gesture);

        auto pos = this->mapFromGlobal(gesture->hotSpot().toPoint());
        auto angle = swipe->swipeAngle();

        if (ui->readerNavigationBar->isVisible())
        {
            if (pos.y() > this->height() * SIZES.readerBottomMenuThreshold * 2)
                showMenuBar(false);
        }
        else if (angle > 155 && angle < 205)
        {
            emit advancPageClicked(conditionalReverse(Forward, settings && settings->reverseSwipeDirection));
        }
        else if (angle > 335 || angle < 25)
        {
            emit advancPageClicked(conditionalReverse(Backward, settings && settings->reverseSwipeDirection));
        }
        else if (swipe->hotSpot().y() < this->height() * SIZES.readerBottomMenuThreshold && angle > 245 &&
                 angle < 295)
        {
            showMenuBar(true);
        }
    }
    else if (QGesture *gesture = event->gesture(Qt::TapGesture))
    {
        auto pos = this->mapFromGlobal(gesture->hotSpot().toPoint());

        if (gesture->state() != Qt::GestureFinished)
            return true;

        if (ui->readerNavigationBar->isVisible())
        {
            if (pos.y() > this->height() * SIZES.readerBottomMenuThreshold * 2)
                showMenuBar(false);
        }
        else if (pos.y() < this->height() * SIZES.readerBottomMenuThreshold ||
                 pos.y() > this->height() * (1.0 - SIZES.readerBottomMenuThreshold))
        {
            showMenuBar(true);
        }
        else
        {
            PageTurnDirection direction =
                pos.x() > this->width() * SIZES.readerPreviousPageThreshold ? Forward : Backward;
            emit advancPageClicked(direction);
        }
    }

    return true;
}

void MangaReaderWidget::updateMenuBar()
{
    QTime now = QTime::currentTime();
    ui->labelTime->setText(now.toString("hh:mm"));

    ui->batteryIcon->updateIcon();
}

void MangaReaderWidget::updateCurrentIndex(const ReadingProgress &progress)
{
    ui->labelReaderChapter->setText("Chapter: " + QString::number(progress.index.chapter + 1) + "/" +
                                    QString::number(progress.numChapters));
    ui->labelReaderPage->setText("Page: " + QString::number(progress.index.page + 1) + "/" +
                                 QString::number(progress.numPages));

    gotodialog->setup(progress);
}

void MangaReaderWidget::on_pushButtonReaderHome_clicked()
{
    emit changeView(HomeTab);
}

void MangaReaderWidget::on_pushButtonReaderBack_clicked()
{
    emit back();
}

void MangaReaderWidget::on_pushButtonReaderFavorites_clicked()
{
    emit changeView(FavoritesTab);
}

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
    else
    {
        ui->mangaImageContainer->clearImage();
    }
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
        auto img = new QPixmap(path);

        if (settings && settings->doublePageFullscreen &&
            (img->width() <= img->height()) != (this->width() <= this->height()))
        {
            QTransform t;
            t = t.rotate(90);

            auto imgRot = new QPixmap(img->transformed(t));
            delete img;
            img = imgRot;
        }

        imgcache.insert(0, {QSharedPointer<QPixmap>(img), path});

        if (imgcache.count() > CONF.imageCacheSize)
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
void MangaReaderWidget::setSettings(Settings *settings)
{
    this->settings = settings;
}

void MangaReaderWidget::setFrontLightPanelState(int lightmin, int lightmax, int light, int comflightmin,
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
    if (gotodialog->exec() == QDialog::Accepted)
    {
        showMenuBar(false);

        emit gotoIndex(gotodialog->selectedindex);
    }
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
        updateMenuBar();
        ui->readerNavigationBar->setVisible(true);
        ui->readerFrontLightBar->setVisible(true);
    }
}
