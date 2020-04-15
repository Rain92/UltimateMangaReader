#include "homewidget.h"

#include "ui_homewidget.h"

HomeWidget::HomeWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::HomeWidget),
      core(nullptr),
      sourcesprogress(),
      filteredmangatitles(),
      filteredmangalinks(),
      filteractive(false)
{
    ui->setupUi(this);
    adjustSizes();

    ui->listViewSources->setModel(new QStandardItemModel(this));
    ui->listViewMangas->setModel(new QStringListModel(this));

    //    for (auto ms : core->activeMangaSources)
    //    {
    //        QObject::connect(ms, &AbstractMangaSource::updateProgress, this, &HomeWidget::updateProgress);
    //        QObject::connect(ms, &AbstractMangaSource::updateError, this, &HomeWidget::updateError);
    //    }

    updateDialog = new UpdateDialog(this);

    QObject::connect(ui->lineEditFilter, &CLineEdit::returnPressed, this,
                     &HomeWidget::on_pushButtonFilter_clicked);
}

HomeWidget::~HomeWidget()
{
    delete ui;
}

void HomeWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateSourcesList();
}

void HomeWidget::setCore(UltimateMangaReaderCore *core)
{
    this->core = core;
}

void HomeWidget::adjustSizes()
{
    ui->pushButtonFilter->setFixedHeight(buttonsize);
    ui->pushButtonFilterClear->setFixedHeight(buttonsize);
    ui->lineEditFilter->setFixedHeight(buttonsize);

    ui->listViewSources->setFixedHeight(listsourcesheight);
    ui->listViewSources->setViewMode(QListView::IconMode);
    ui->listViewSources->setIconSize(QSize(mangasourceiconsize, mangasourceiconsize));
    ui->listViewSources->setStyleSheet("font-size: 8pt");
    ui->listViewMangas->setUniformItemSizes(true);

    ui->listViewMangas->setFocusPolicy(Qt::FocusPolicy::NoFocus);
}

void HomeWidget::updateSourcesList()
{
    auto model = dynamic_cast<QStandardItemModel *>(ui->listViewSources->model());
    model->clear();

    for (auto ms : core->activeMangaSources)
        model->appendRow(listViewItemfromMangaSource(ms));

    refreshMangaListView();
}

void HomeWidget::updateError(const QString &error)
{
    AbstractMangaSource *src = dynamic_cast<AbstractMangaSource *>(sender());
    if (src != nullptr)
        updateDialog->error("Error updating " + src->name + ": \n" + error);
    else
        updateDialog->error("Error updating: \n" + error);
}

void HomeWidget::updateProgress(int progress)
{
    sourcesprogress[(AbstractMangaSource *)sender()] = progress;

    int sum = std::accumulate(sourcesprogress.begin(), sourcesprogress.end(), 0);

    updateDialog->updateProgress(sum);
}

void HomeWidget::on_pushButtonUpdate_clicked()
{
    sourcesprogress.clear();

    updateDialog->setup(core->activeMangaSources.count() * 100, "Updating Mangalists");

    updateDialog->show();

    for (auto ms : core->activeMangaSources)
    {
        updateDialog->setLabelText("Updating " + ms->name);
        auto mangaList = ms->getMangaList();
        if (mangaList.nominalSize == mangaList.actualSize)
        {
            mangaList.sortAndFilter();
            ms->mangaList = mangaList;
            ms->serializeMangaList();
        }
        else
        {
            updateError("Number of mangas does not match.\n" + QString::number(mangaList.actualSize) +
                        " vs " + QString::number(mangaList.nominalSize));
        }
    }
}

QList<QStandardItem *> HomeWidget::listViewItemfromMangaSource(AbstractMangaSource *source)
{
    QList<QStandardItem *> items;
    QStandardItem *item = new QStandardItem(source->name);
    item->setData(source->name);
    item->setIcon(QIcon(QPixmap(":/images/mangahostlogos/" + source->name.toLower() + ".png")));
    item->setSizeHint(QSize(mangasourceitemwidth, mangasourceitemheight));
    items.append(item);

    return items;
}

void HomeWidget::on_listViewSources_clicked(const QModelIndex &index)
{
    auto currentsource = core->activeMangaSources[index.data().toString()];

    emit mangaSourceClicked(currentsource);
}

void HomeWidget::currentMangaSourceChanged()
{
    refreshMangaListView();
    if (ui->lineEditFilter->text() != "")
        on_pushButtonFilter_clicked();
}

void HomeWidget::on_pushButtonFilter_clicked()
{
    if (core->currentMangaSource == nullptr || core->currentMangaSource->mangaList.actualSize == 0)
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

    for (int i = 0; i < core->currentMangaSource->mangaList.titles.size(); i++)
        if (core->currentMangaSource->mangaList.titles[i].contains(ss, Qt::CaseInsensitive))
        {
            filteredmangatitles.append(core->currentMangaSource->mangaList.titles[i]);
            filteredmangalinks.append(core->currentMangaSource->mangaList.links[i]);
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
    auto model = dynamic_cast<QStringListModel *>(ui->listViewMangas->model());

    if (core->currentMangaSource == nullptr)
    {
        model->setStringList({});
        return;
    }

    if (!filteractive)
        model->setStringList(core->currentMangaSource->mangaList.titles);
    else
        model->setStringList(filteredmangatitles);
}

void HomeWidget::on_listViewMangas_clicked(const QModelIndex &index)
{
    int idx = index.row();

    QString mangalink =
        filteractive ? filteredmangalinks[idx] : core->currentMangaSource->mangaList.links[idx];

    if (!core->currentMangaSource->mangaList.absoluteUrls)
        mangalink.prepend(core->currentMangaSource->baseurl);

    QString mangatitle =
        filteractive ? filteredmangatitles[idx] : core->currentMangaSource->mangaList.titles[idx];

    emit mangaClicked(mangalink, mangatitle);
}
