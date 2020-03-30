#include "homewidget.h"

#include "ui_homewidget.h"

HomeWidget::HomeWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::HomeWidget),
      currentsource(nullptr),
      filteredmangatitles(),
      filteredmangalinks(),
      filteractive(false)
{
    ui->setupUi(this);
    adjustSizes();

    updatedialog = new UpdateDialog(this);
    clearcachedialog = new ClearCacheDialog(this);

    QObject::connect(ui->lineEditFilter, SIGNAL(returnPressed()),
                     ui->pushButtonFilter, SIGNAL(clicked()));

    QObject::connect(updatedialog, SIGNAL(retry()), this,
                     SLOT(on_pushButtonUpdate_clicked()));
    QObject::connect(clearcachedialog, SIGNAL(clearCache(int)), this,
                     SLOT(clearCacheDialogButtonClicked(int)));
}

HomeWidget::~HomeWidget() { delete ui; }

void HomeWidget::setMangaSources(const QList<AbstractMangaSource *> &sources)
{
    mangasources = sources;

    setupSourcesList();
}

void HomeWidget::adjustSizes()
{
    ui->pushButtonFilter->setMinimumHeight(buttonsize);
    ui->pushButtonUpdate->setMinimumHeight(buttonsize);
    ui->pushButtonClearCache->setMinimumHeight(buttonsize);
    ui->pushButtonFilterClear->setMinimumHeight(buttonsize);
    ui->lineEditFilter->setMinimumHeight(buttonsize);

    ui->listViewSources->setMinimumHeight(listsourcesheight);
    ui->listViewSources->setMinimumWidth(listsourceswidth);

    ui->listViewSources->setVerticalScrollBar(
        new CScrollBar(Qt::Vertical, ui->listViewSources));
    ui->listViewMangas->setVerticalScrollBar(
        new CScrollBar(Qt::Vertical, ui->listViewMangas));
    ui->listViewMangas->setHorizontalScrollBar(
        new CScrollBar(Qt::Horizontal, ui->listViewMangas));
    ui->listViewMangas->setUniformItemSizes(true);
}

void HomeWidget::setupSourcesList()
{
    ui->listViewSources->setViewMode(QListView::IconMode);
    QStandardItemModel *model = new QStandardItemModel(this);

    for (auto ms : mangasources)
        model->appendRow(listViewItemfromMangaSource(ms));

    ui->listViewSources->setIconSize(
        QSize(mangasourceiconsize, mangasourceiconsize));

    ui->listViewSources->setStyleSheet("QListView { font-size: 8pt; }");

    ui->listViewSources->setModel(model);

    for (auto ms : mangasources)
    {
        QObject::connect(ms, SIGNAL(updateProgress(int)), this,
                         SLOT(updateProgress(int)));
        QObject::connect(ms, SIGNAL(updateError(QString)), this,
                         SLOT(updateError(QString)));
    }
}

void HomeWidget::updateError(const QString &error)
{
    AbstractMangaSource *src = dynamic_cast<AbstractMangaSource *>(sender());
    if (src != nullptr)
        updatedialog->error("Error updating " + src->name + ": \n" + error);
    else
        updatedialog->error("Error updating: \n" + error);
}

void HomeWidget::updateProgress(int p)
{
    int sind = mangasources.indexOf((AbstractMangaSource *)sender());

    sourcesprogress[sind] = p;

    int sum =
        std::accumulate(sourcesprogress.begin(), sourcesprogress.end(), 0);

    updatedialog->updateProgress(sum);
}

void HomeWidget::on_pushButtonUpdate_clicked()
{
    sourcesprogress = QVector<int>(mangasources.count());

    updatedialog->setup(mangasources.count() * 100, "Updating Mangalists");

    updatedialog->show();

    for (auto ms : mangasources)
    {
        updatedialog->setLabelText("Updating " + ms->name);
        auto mangaList = ms->getMangaList();
        if (mangaList.nominalSize == mangaList.actualSize)
        {
            mangaList.sortAndFilter();
            ms->mangaList = mangaList;
            ms->serializeMangaList();
        }
        else
        {
            updateError("Number of mangas does not match.\n" +
                        QString::number(mangaList.actualSize) + " vs " +
                        QString::number(mangaList.nominalSize));
        }
    }
}

QList<QStandardItem *> HomeWidget::listViewItemfromMangaSource(
    AbstractMangaSource *source)
{
    QList<QStandardItem *> items;
    QStandardItem *item = new QStandardItem(source->name);
    item->setIcon(QIcon(QPixmap(":/resources/images/mangahostlogos/" +
                                source->name.toLower() + ".png")));
    item->setSizeHint(QSize(mangasourceitemwidth, mangasourceitemheight));
    items.append(item);

    return items;
}

bool removeDir(const QString &dirName, const QString &ignore = "")
{
    bool result = true;
    QDir dir(dirName);

    if (dir.exists())
    {
        foreach (
            QFileInfo info,
            dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System |
                                  QDir::Hidden | QDir::AllDirs | QDir::Files,
                              QDir::DirsFirst))
        {
            if (info.isDir())
            {
                result = removeDir(info.absoluteFilePath(), ignore);
            }
            else
            {
                if (ignore == "" || !info.absoluteFilePath().endsWith(ignore))
                    result = QFile::remove(info.absoluteFilePath());
            }

            if (!result)
            {
                return result;
            }
        }
        // result = dir.rmdir(dirName);
    }
    return result;
}

void HomeWidget::clearCacheDialogButtonClicked(int level)
{
    switch (level)
    {
        case 1:
            for (auto ms : mangasources)
            {
                foreach (
                    QFileInfo info,
                    QDir(CONF.cacheDir + ms->name)
                        .entryInfoList(QDir::NoDotAndDotDot | QDir::System |
                                       QDir::Hidden | QDir::AllDirs))
                    removeDir(info.absoluteFilePath() + "/images");
            }
            break;

        case 2:
            for (auto ms : mangasources)
                removeDir(CONF.cacheDir + ms->name, "progress.dat");

            break;

        case 3:
            removeDir(CONF.cacheDir, "mangaList.dat");
            emit favoritesCleared();
            break;

        default:
            break;
    }
}

void HomeWidget::on_pushButtonClearCache_clicked()
{
    clearcachedialog->show();
    clearcachedialog->getCacheSize();
}

void HomeWidget::on_listViewSources_clicked(const QModelIndex &index)
{
    currentsource = mangasources.at(index.row());
    refreshMangaListView();

    emit mangaSourceClicked(currentsource);

    if (ui->lineEditFilter->text() != "")
        on_pushButtonFilter_clicked();
}

void HomeWidget::on_pushButtonFilter_clicked()
{
    if (currentsource == nullptr || currentsource->mangaList.actualSize == 0)
        return;

    QString ss = ui->lineEditFilter->text();

    filteredmangalinks.clear();
    filteredmangatitles.clear();

    if (ss == "")
    {
        filteractive = false;
        refreshMangaListView();
        return;
    }

    for (int i = 0; i < currentsource->mangaList.titles.size(); i++)
        if (currentsource->mangaList.titles[i].contains(ss,
                                                        Qt::CaseInsensitive))
        {
            filteredmangatitles.append(currentsource->mangaList.titles[i]);
            filteredmangalinks.append(currentsource->mangaList.links[i]);
        }

    filteractive = true;
    refreshMangaListView();
}

void HomeWidget::on_pushButtonFilterClear_clicked()
{
    if (ui->lineEditFilter->text() != "")
        ui->lineEditFilter->clear();

    filteredmangalinks.clear();
    filteredmangatitles.clear();

    filteractive = false;
    refreshMangaListView();
}

void HomeWidget::refreshMangaListView()
{
    if (currentsource == nullptr)
        return;

    QStringListModel *model = new QStringListModel(this);

    if (!filteractive)
        model->setStringList(currentsource->mangaList.titles);
    else
        model->setStringList(filteredmangatitles);

    if (ui->listViewMangas->model() != nullptr)
        ui->listViewMangas->model()->deleteLater();

    ui->listViewMangas->setModel(model);
}

void HomeWidget::on_listViewMangas_clicked(const QModelIndex &index)
{
    int idx = index.row();

    QString mangalink = filteractive ? filteredmangalinks[idx]
                                     : currentsource->mangaList.links[idx];

    if (!currentsource->mangaList.absoluteUrls)
        mangalink.prepend(currentsource->baseurl);

    QString mangatitle = filteractive ? filteredmangatitles[idx]
                                      : currentsource->mangaList.titles[idx];

    emit mangaClicked(mangalink, mangatitle);
}
