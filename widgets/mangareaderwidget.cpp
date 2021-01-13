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

    connect(ui->toolButtonLessLight, &QToolButton::clicked,
            [this]() { ui->horizontalSliderLight->setValue(ui->horizontalSliderLight->value() - 1); });
    connect(ui->toolButtonMoreLight, &QToolButton::clicked,
            [this]() { ui->horizontalSliderLight->setValue(ui->horizontalSliderLight->value() + 1); });
    connect(ui->toolButtonLessComfLight, &QToolButton::clicked, [this]() {
        ui->horizontalSliderComfLight->setValue(ui->horizontalSliderComfLight->value() + 1);
    });
    connect(ui->toolButtonMoreComfLight, &QToolButton::clicked, [this]() {
        ui->horizontalSliderComfLight->setValue(ui->horizontalSliderComfLight->value() - 1);
    });

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

    ui->toolButtonLessLight->setFixedSize(QSize(SIZES.lightIconSize, SIZES.lightIconSize));
    ui->toolButtonMoreLight->setFixedSize(QSize(SIZES.lightIconSize, SIZES.lightIconSize));
    ui->toolButtonLessComfLight->setFixedSize(QSize(SIZES.lightIconSize, SIZES.lightIconSize));
    ui->toolButtonMoreComfLight->setFixedSize(QSize(SIZES.lightIconSize, SIZES.lightIconSize));

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
        emit advancPageClicked(
            conditionalReverse(Forward, settings->buttonAdvance != AdvancePageHWButton::Up));
    else if (event->key() == Qt::Key_PageDown)
        emit advancPageClicked(
            conditionalReverse(Forward, settings->buttonAdvance != AdvancePageHWButton::Down));
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
            emit advancPageClicked(
                conditionalReverse(Forward, settings->swipeAdvance != AdvancePageGestureDirection::Left));
        }
        else if (angle > 335 || angle < 25)
        {
            emit advancPageClicked(
                conditionalReverse(Forward, settings->swipeAdvance != AdvancePageGestureDirection::Right));
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
            auto tabSide = pos.x() < this->width() / 2 ? Left : Right;
            if (pos.x() > this->width() * SIZES.readerPageSideThreshold &&
                pos.x() < this->width() * (1 - SIZES.readerPageSideThreshold))
                tabSide = settings->tabAdvance;

            emit advancPageClicked(conditionalReverse(Forward, settings->tabAdvance != tabSide));
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

    if (QFile::exists(path))
    {
        int i = searchCache(path);

        if (i != -1)
        {
            qDebug() << "Cachehit:" << i;
            ui->mangaImageContainer->setImage(imgcache[i].first);
        }
        else
        {
            if (addImageToCache(path, false))
            {
                i = searchCache(path);
                ui->mangaImageContainer->setImage(imgcache[i].first);
            }
            else
            {
                ui->mangaImageContainer->showErrorImage();
            }
        }
    }
    else
    {
        ui->mangaImageContainer->showErrorImage();
    }
}

void MangaReaderWidget::checkMem()
{
    while (imgcache.size() > 1 && !enoughFreeSystemMemory())
    {
        //        int free = getFreeSystemMemory() / 1024 / 1024;
        //        qDebug() << "Free(MB):" << free;
        imgcache.removeLast();
        //        free = getFreeSystemMemory() / 1024 / 1024;
        //        qDebug() << "Freed memory!" << free;
    }
}

bool MangaReaderWidget::addImageToCache(const QString &path, QSharedPointer<QImage> img)
{
    //    QElapsedTimer t;
    //    t.start();

    int i = searchCache(path);
    if (i != -1)
    {
        imgcache.move(i, 0);
    }
    else
    {
        if (!img || img->isNull())
            return false;

        imgcache.insert(0, {img, path});

        if (imgcache.count() > CONF.imageCacheSize)
            imgcache.removeLast();
    }

    checkMem();

    //    qDebug() << "Load image1:" << t.elapsed() << path.split('/').last();

    return true;
}

bool MangaReaderWidget::addImageToCache(const QString &path, bool isPreload)
{
    //    QElapsedTimer t;
    //    t.start();

    int i = searchCache(path);
    if (i != -1)
    {
        imgcache.move(i, 0);
    }
    else if (isPreload && !enoughFreeSystemMemory())
    {
        return false;
    }
    else
    {
        auto img = QSharedPointer<QImage>(new QImage(path));

        if (!img || img->isNull())
            return false;

        imgcache.insert(0, {img, path});

        if (imgcache.count() > CONF.imageCacheSize)
            imgcache.removeLast();
    }

    checkMem();

    //    qDebug() << "Load image2:" << t.elapsed() << path.split('/').last();

    return true;
}

int MangaReaderWidget::searchCache(const QString &path) const
{
    for (int i = 0; i < imgcache.size(); i++)
        if (imgcache[i].second == path)
            return i;

    return -1;
}

void MangaReaderWidget::clearCache()
{
    imgcache.clear();
    ui->mangaImageContainer->clearImage();
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
    ui->horizontalSliderLight->setPageStep((lightmax - lightmin) / 20);
    ui->horizontalSliderLight->setValue(light);

    if (comflightmin != comflightmax)
    {
        ui->horizontalSliderComfLight->setMinimum(comflightmin);
        ui->horizontalSliderComfLight->setMaximum(comflightmax);
        ui->horizontalSliderComfLight->setPageStep((comflightmax - comflightmin) / 20);
        ui->horizontalSliderComfLight->setValue(comflight);
    }
    else
    {
        ui->horizontalSliderComfLight->hide();
        ui->toolButtonLessComfLight->hide();
        ui->toolButtonMoreComfLight->hide();
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
