#include "homewidget.h"

#include "ui_homewidget.h"

HomeWidget::HomeWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::HomeWidget),
      currentMangaSource(nullptr),
      filteredMangaTitles(),
      filteredMangaUrls(),
      filteractive(false)
{
    ui->setupUi(this);
    adjustUI();

    ui->listViewSources->setModel(new QStandardItemModel(this));
    ui->listViewMangas->setModel(new QStringListModel(this));

    activateScroller(ui->listViewSources);
    activateScroller(ui->listViewMangas);

    QObject::connect(ui->lineEditFilter, &CLineEdit::returnPressed, this,
                     &HomeWidget::on_pushButtonFilter_clicked);
}

HomeWidget::~HomeWidget()
{
    delete ui;
}

void HomeWidget::adjustUI()
{
    ui->pushButtonFilter->setProperty("type", "borderless");
    ui->pushButtonFilterClear->setProperty("type", "borderless");

    ui->pushButtonFilter->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonFilterClear->setFixedHeight(SIZES.buttonSize);

    ui->listViewSources->setFixedHeight(SIZES.listSourcesHeight);
    ui->listViewSources->setIconSize(QSize(SIZES.mangasourceIconSize, SIZES.mangasourceIconSize));

    ui->listViewMangas->setFocusPolicy(Qt::FocusPolicy::NoFocus);
}

void HomeWidget::updateSourcesList(const QList<AbstractMangaSource *> &sources)
{
    auto model = dynamic_cast<QStandardItemModel *>(ui->listViewSources->model());
    model->clear();

    for (auto ms : sources)
        model->appendRow(listViewItemfromMangaSource(ms));

    refreshMangaListView();
}

void HomeWidget::updateMangaSourceSelection(int row)
{
    auto model = dynamic_cast<QStandardItemModel *>(ui->listViewSources->model());
    for (int i = 0; i < model->rowCount(); i++)
    {
        QStandardItem *item = model->item(i);
        auto f = item->font();
        f.setUnderline(i == row);
        item->setFont(f);
    }
}

QList<QStandardItem *> HomeWidget::listViewItemfromMangaSource(AbstractMangaSource *source)
{
    QList<QStandardItem *> items;
    QStandardItem *item = new QStandardItem(source->name);
    item->setData(QVariant::fromValue(static_cast<void *>(source)));
    item->setIcon(QIcon(QPixmap(":/images/mangahostlogos/" + source->name.toLower() + ".png")));
    item->setSizeHint(QSize(SIZES.mangasourceItemWidth, SIZES.mangasourceItemHeight));
    item->setText(source->name);
    items.append(item);

    return items;
}

void HomeWidget::on_listViewSources_clicked(const QModelIndex &index)
{
    updateMangaSourceSelection(index.row());

    auto clickedsource = static_cast<AbstractMangaSource *>(index.data(Qt::UserRole + 1).value<void *>());
    emit mangaSourceClicked(clickedsource);
}

void HomeWidget::currentMangaSourceChanged(AbstractMangaSource *source)
{
    currentMangaSource = source;

    refreshMangaListView();
    if (ui->lineEditFilter->text() != "")
        on_pushButtonFilter_clicked();
}

void HomeWidget::on_pushButtonFilter_clicked()
{
    if (currentMangaSource == nullptr || currentMangaSource->mangaList.size == 0)
        return;

    QString ss = ui->lineEditFilter->text();

    filteredMangaUrls.clear();
    filteredMangaTitles.clear();

    if (ss == "")
    {
        filteractive = false;
        refreshMangaListView();
        return;
    }

    for (int i = 0; i < currentMangaSource->mangaList.titles.size(); i++)
        if (currentMangaSource->mangaList.titles[i].contains(ss, Qt::CaseInsensitive))
        {
            filteredMangaTitles.append(currentMangaSource->mangaList.titles[i]);
            filteredMangaUrls.append(currentMangaSource->mangaList.urls[i]);
        }

    filteractive = true;
    refreshMangaListView();
}

void HomeWidget::on_pushButtonFilterClear_clicked()
{
    if (ui->lineEditFilter->text() != "")
        ui->lineEditFilter->clear();

    filteredMangaUrls.clear();
    filteredMangaTitles.clear();

    filteractive = false;
    refreshMangaListView();
}

void HomeWidget::refreshMangaListView()
{
    auto model = dynamic_cast<QStringListModel *>(ui->listViewMangas->model());

    if (currentMangaSource == nullptr)
    {
        model->setStringList({});
        return;
    }

    if (!filteractive)
        model->setStringList(currentMangaSource->mangaList.titles);
    else
        model->setStringList(filteredMangaTitles);

    ui->listViewMangas->verticalScrollBar()->setValue(0);
}

void HomeWidget::on_listViewMangas_clicked(const QModelIndex &index)
{
    int idx = index.row();

    QString mangaUrl = filteractive ? filteredMangaUrls[idx] : currentMangaSource->mangaList.urls[idx];

    if (!currentMangaSource->mangaList.absoluteUrls)
        mangaUrl.prepend(currentMangaSource->baseurl);

    QString mangatitle = filteractive ? filteredMangaTitles[idx] : currentMangaSource->mangaList.titles[idx];

    emit mangaClicked(mangaUrl, mangatitle);
}
