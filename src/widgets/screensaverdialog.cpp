#include "screensaverdialog.h"

#include "ui_screensaverdialog.h"

ScreensaverDialog::ScreensaverDialog(QWidget* parent) : QDialog(parent), ui(new Ui::ScreensaverDialog)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Popup);

    ui->labelImage->setStyleSheet("QLabel { background-color : black; }");
}
ScreensaverDialog::~ScreensaverDialog()
{
    delete ui;
}

bool ScreensaverDialog::event(QEvent* event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        parent()->event(event);
    return QWidget::event(event);
}

void ScreensaverDialog::showRandomScreensaver()
{
    this->setGeometry(static_cast<QWidget*>(this->parent())->geometry());

    QDir dir(CONF.screensaverDir);

    if (!dir.exists())
        return;

    auto files = dir.entryList({"*.jpg", "*.png"});

    if (files.count() > 0)
    {
        int i = QRandomGenerator::global()->generate() % files.count();

        auto fp = CONF.screensaverDir + files[i];

        QFile file(fp);
        file.open(QIODevice::ReadOnly);

        QImage img =
            processImageN(file.readAll(), "", this->size(), DoublePageNoRotation, false, false, true);

        QPixmap pic = QPixmap::fromImage(img);
        ui->labelImage->setPixmap(pic);
        file.close();
    }

    ui->labelImage->showFullScreen();

    open();
}
