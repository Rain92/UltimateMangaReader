#include "homewidget.h"
#include "ui_homewidget.h"
#include "configs.h"
#include "cscrollbar.h"
#include <QLabel>

HomeWidget::HomeWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::HomeWidget),
    currentsource(nullptr),
    filteredmangatitles(),
    filteredmangalinks()
{
    ui->setupUi(this);
    adjustSizes();

    updatedialog = new UpdateDialog(this);
    clearcachedialog = new ClearCacheDialog(this);

    QObject::connect(ui->lineEditFilter, SIGNAL(returnPressed()), ui->pushButtonFilter, SIGNAL(clicked()));

    QObject::connect(updatedialog, SIGNAL(retry()), this, SLOT(on_pushButtonUpdate_clicked()));
    QObject::connect(clearcachedialog, SIGNAL(clearCache(int)), this, SLOT(clearCacheDialogButtonClicked(int)));

}

HomeWidget::~HomeWidget()
{
    delete ui;
}

void HomeWidget::setMangaSources(QList<AbstractMangaSource *> *sources)
{
    mangasources = sources;

    setupSourcesList();

//    currentsource = sources->at(0);
}

void  HomeWidget::adjustSizes()
{
    ui->pushButtonFilter->setMinimumHeight(buttonsize);
    ui->pushButtonUpdate->setMinimumHeight(buttonsize);
    ui->pushButtonClearCache->setMinimumHeight(buttonsize);
    ui->pushButtonFilterClear->setMinimumHeight(buttonsize);
    ui->lineEditFilter->setMinimumHeight(buttonsize);

    ui->listViewSources->setMinimumHeight(listsourcesheight);
    ui->listViewSources->setMinimumWidth(listsourceswidth);

    ui->listViewSources->setVerticalScrollBar(new CScrollBar(Qt::Vertical, ui->listViewSources));
    ui->listViewMangas->setVerticalScrollBar(new CScrollBar(Qt::Vertical, ui->listViewMangas));
    ui->listViewMangas->setHorizontalScrollBar(new CScrollBar(Qt::Horizontal, ui->listViewMangas));
    ui->listViewMangas->setUniformItemSizes(true);
}

void  HomeWidget::setupSourcesList()
{
    ui->listViewSources->setViewMode(QListView::IconMode);
    QStandardItemModel *model = new QStandardItemModel(this);

    foreach (AbstractMangaSource *ms, *mangasources)
        model->appendRow(*listViewItemfromMangaSource(ms));

    ui->listViewSources->setIconSize(QSize(mangasourceiconsize, mangasourceiconsize));

    ui->listViewSources->setStyleSheet("QListView { font-size: 8pt; }");

    ui->listViewSources->setModel(model);

    foreach (AbstractMangaSource *src, *mangasources)
    {
        QObject::connect(src, SIGNAL(updateProgress(int)), this, SLOT(updateProgress(int)));
        QObject::connect(src, SIGNAL(updateError(QString)), this, SLOT(updateError(QString)));
    }
}


void HomeWidget::updateError(const QString &error)
{
    AbstractMangaSource *src = static_cast<AbstractMangaSource *>(sender());
    updatedialog->error("Error updating " + src->name + ": \n" + error);
}

void  HomeWidget::updateProgress(int p)
{
    int sind = mangasources->indexOf((AbstractMangaSource *)sender());

    sourcesprogress[sind] = p;

    int sum = 0;
    foreach (int sp, sourcesprogress)
        sum += sp;

    updatedialog->updateProgress(sum);
}


void HomeWidget::on_pushButtonUpdate_clicked()
{
    sourcesprogress = QVector<int>(mangasources->count());

    updatedialog->setup(mangasources->count() * 100, "Updating Mangalists");

    updatedialog->show();


    foreach (AbstractMangaSource *ms, *mangasources)
    {
        updatedialog->setLabelText("Updating " + ms->name);
        if (!ms->updateMangaList())
            return;

        if (ms->nummangas != ms->mangalist.links.count())
            updateError("Number of mangas does not match.");

        ms->serializeMangaList();
    }

}

QList<QStandardItem *> *HomeWidget::listViewItemfromMangaSource(AbstractMangaSource *source)
{
    QList<QStandardItem *> *items = new QList<QStandardItem *> ();
    QStandardItem *item = new QStandardItem(source->name);
    item->setIcon(QIcon(QPixmap(":/resources/images/mangahostlogos/" + source->name.toLower() + ".png")));
    item->setSizeHint(QSize(mangasourceitemwidth, mangasourceitemheight));
    items->append(item);

    return items;
}

bool removeDir(const QString &dirName, const QString &ignore = "")
{
    bool result = true;
    QDir dir(dirName);

//    qDebug() << dir.absoluteFilePath(dirName);
    if (dir.exists())
    {
//        qDebug() << dirName;
        foreach (QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst))
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
        //result = dir.rmdir(dirName);
    }
    return result;
}

void HomeWidget::clearCacheDialogButtonClicked(int level)
{
    switch (level)
    {
    case 1:
        foreach (AbstractMangaSource *s, *mangasources)
        {
            foreach (QFileInfo info, QDir(cachedir + s->name).entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs))
                removeDir(info.absoluteFilePath() + "/images");
        }
        break;

    case 2:
        foreach (AbstractMangaSource *s, *mangasources)
            removeDir(cachedir + s->name, "progress.dat");

        break;

    case 3:
        removeDir(cachedir, "mangalist.dat");
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
    if (ui->lineEditFilter->text() != "")
        ui->lineEditFilter->clear();

    filteredmangalinks.clear();
    filteredmangatitles.clear();

    currentsource = mangasources->at(index.row());
    refreshMangaListView();

    emit mangaSourceClicked(currentsource);
}



void HomeWidget::on_pushButtonFilter_clicked()
{
    if (currentsource == nullptr || currentsource->nummangas == 0)
        return;

    QString ss = ui->lineEditFilter->text();

    filteredmangalinks.clear();
    filteredmangatitles.clear();

    if (ss == "")
    {
        refreshMangaListView();
        return;
    }

    for (int i = 0; i < currentsource->nummangas; i++)
        if (currentsource->mangalist.titles[i].contains(ss, Qt::CaseInsensitive))
        {
            filteredmangatitles.append(currentsource->mangalist.titles[i]);
            filteredmangalinks.append(currentsource->mangalist.links[i]);
        }

    refreshMangaListView();
}

void HomeWidget::on_pushButtonFilterClear_clicked()
{

    if (ui->lineEditFilter->text() != "")
        ui->lineEditFilter->clear();

    filteredmangalinks.clear();
    filteredmangatitles.clear();

    refreshMangaListView();
}

void HomeWidget::refreshMangaListView()
{
    if (currentsource == nullptr)
        return;

    QStringListModel *model = new QStringListModel(this);

    if (filteredmangatitles.empty())
        model->setStringList(currentsource->mangalist.titles);
    else
        model->setStringList(filteredmangatitles);

    if (ui->listViewMangas->model() != nullptr)
        ui->listViewMangas->model()->removeRows(0, ui->listViewMangas->model()->rowCount());
    ui->listViewMangas->setModel(model);

}


void HomeWidget::on_listViewMangas_clicked(const QModelIndex &index)
{
    QString mangalink;
    QString mangatitle;

    if (filteredmangalinks.count() == 0)
    {
        mangalink = currentsource->baseurl + currentsource->mangalist.links[index.row()];
        mangatitle = currentsource->mangalist.titles[index.row()];
    }
    else
    {
        mangalink = currentsource->baseurl + filteredmangalinks[index.row()];
        mangatitle = filteredmangatitles[index.row()];
    }

    emit mangaClicked(mangalink, mangatitle);
//    currentmanga = currentsource->getMangaInfo(mangalink);
}

