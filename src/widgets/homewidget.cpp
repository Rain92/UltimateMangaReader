#include "homewidget.h"

#include "ui_homewidget.h"
#include "widgetutils.h"

HomeWidget::HomeWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::HomeWidget),
      mangaLists(nullptr),
      currentMangaSource(nullptr),
      filteredMangaTitles(),
      filteredMangaUrls(),
      filteractive(false)
{
    ui->setupUi(this);
    adjustUI();

    // after the focus change is done
    connect(ui->lineEditFilter, &CLineEdit::keyboardRequested, this,
            [this]() { emit virtualKeyboardRequested(true); }, Qt::QueuedConnection);
    connect(ui->lineEditFilter, &CLineEdit::keyboardDismissed, this,
            [this]() { emit virtualKeyboardRequested(false); }, Qt::QueuedConnection);

    ui->listViewSources->setModel(new QStandardItemModel(this));
    ui->listViewMangas->setModel(new QStringListModel(this));

    QObject::connect(ui->lineEditFilter, &CLineEdit::returnPressed, this,
                     &HomeWidget::on_pushButtonFilter_clicked);
}

HomeWidget::~HomeWidget()
{
    delete ui;
}

void HomeWidget::adjustUI()
{
    activateScroller(ui->listViewSources);
    activateScroller(ui->listViewMangas);

    ui->pushButtonFilter->setProperty("type", "borderless");
    ui->pushButtonFilterClear->setProperty("type", "borderless");

    ui->pushButtonFilter->setFixedHeight(SIZES.buttonSize);
    ui->pushButtonFilterClear->setFixedHeight(SIZES.buttonSize);

    ui->listViewSources->setFixedHeight(SIZES.listSourcesHeight);
    ui->listViewSources->setIconSize(QSize(SIZES.mangasourceIconSize, SIZES.mangasourceIconSize));

    ui->listViewMangas->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    ui->listViewMangas->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->listViewMangas->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

void HomeWidget::updateSourcesList(const QList<MangaSourceBase *> &sources)
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

QList<QStandardItem *> HomeWidget::listViewItemfromMangaSource(MangaSourceBase *source)
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

    auto clickedsource = static_cast<MangaSourceBase *>(index.data(Qt::UserRole + 1).value<void *>());
    emit mangaSourceClicked(clickedsource);
}

void HomeWidget::currentMangaSourceChanged(MangaSourceBase *source)
{
    currentMangaSource = source;

    showMangaList();
}

MangaList &HomeWidget::currentList()
{
    return mangaLists->list(currentMangaSource->name);
}

void HomeWidget::mangaListChanged(MangaSourceBase *source)
{
    if (source == currentMangaSource)
        showMangaList();
}

void HomeWidget::showMangaList()
{
    // the results of the previous source (or list)
    filteredMangaUrls.clear();
    filteredMangaTitles.clear();
    filteractive = false;

    refreshMangaListView();
    if (ui->lineEditFilter->text() != "")
        on_pushButtonFilter_clicked();
}

void HomeWidget::on_pushButtonFilter_clicked()
{
    if (currentMangaSource == nullptr)
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

    for (int i = 0; i < currentList().titles.size(); i++)
        if (currentList().titles[i].contains(ss, Qt::CaseInsensitive))
        {
            filteredMangaTitles.append(currentList().titles[i]);
            filteredMangaUrls.append(currentList().urls[i]);
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
        model->setStringList(currentList().titles);
    else
        model->setStringList(filteredMangaTitles);

    ui->listViewMangas->verticalScrollBar()->setValue(0);
}

void HomeWidget::on_listViewMangas_clicked(const QModelIndex &index)
{
    int idx = index.row();
    if (!currentMangaSource || idx < 0 ||
        idx >= (filteractive ? filteredMangaUrls.size() : currentList().urls.size()))
        return;

    QString mangaUrl = filteractive ? filteredMangaUrls[idx] : currentList().urls[idx];

    if (!currentList().absoluteUrls)
        mangaUrl.prepend(currentMangaSource->baseUrl);

    QString mangatitle = filteractive ? filteredMangaTitles[idx] : currentList().titles[idx];

    emit mangaClicked(mangaUrl, mangatitle);
}
