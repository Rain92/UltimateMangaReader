#include "clearcachedialog.h"

#include "ui_clearcachedialog.h"

ClearCacheDialog::ClearCacheDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::ClearCacheDialog)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Popup);

    ui->pushButtonCancel->setFixedHeight(buttonsize);
    ui->pushButtonClear1->setFixedHeight(buttonsize);
    ui->pushButtonClear2->setFixedHeight(buttonsize);
    ui->pushButtonClear3->setFixedHeight(buttonsize);
}

ClearCacheDialog::~ClearCacheDialog() { delete ui; }

long dirSize(QString dirPath)
{
    long size = 0;
    QDir dir(dirPath);
    // calculate total size of current directories' files
    QDir::Filters fileFilters = QDir::Files | QDir::System | QDir::Hidden;
    for (const auto& filePath : dir.entryList(fileFilters))
    {
        QFileInfo fi(dir, filePath);
        size += fi.size();
    }
    // add size of child directories recursively
    QDir::Filters dirFilters =
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden;

    for (const auto& childDirPath : dir.entryList(dirFilters))
        size += dirSize(dirPath + QDir::separator() + childDirPath);
    return size;
}

void ClearCacheDialog::getCacheSize()
{
    long size = dirSize(CONF.cacheDir) / 1024 / 1024;

    ui->labelCacheSize->setText("The cache takes up " + QString::number(size) +
                                " MB.");
}

void ClearCacheDialog::on_pushButtonCancel_clicked() { close(); }

void ClearCacheDialog::on_pushButtonClear1_clicked()
{
    close();
    emit clearCache(1);
}

void ClearCacheDialog::on_pushButtonClear2_clicked()
{
    close();
    emit clearCache(2);
}

void ClearCacheDialog::on_pushButtonClear3_clicked()
{
    close();
    emit clearCache(3);
}
