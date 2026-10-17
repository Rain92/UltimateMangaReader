#include "homewidget.h"

#include "ui_homewidget.h"
#include "widgetutils.h"

HomeWidget::HomeWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::HomeWidget),
      mangaLists(nullptr),
      sources(nullptr),
      currentSource(),
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

void HomeWidget::setSources(SourceRegistry *registry)
{
    sources = registry;
    connect(sources, &SourceRegistry::sourcesChanged, this, &HomeWidget::updateSourcesList);
    connect(sources, &SourceRegistry::currentChanged, this, &HomeWidget::currentSourceChanged);
    updateSourcesList();
    currentSourceChanged(sources->current());
}

void HomeWidget::updateSourcesList()
{
    auto model = dynamic_cast<QStandardItemModel *>(ui->listViewSources->model());
    model->clear();

    for (const auto &name : sources->enabledNames())
    {
        auto source = sources->source(name);
        auto item = new QStandardItem(sourceLogo(name, source ? source->iconPath : QString(),
                                                 SIZES.mangasourceIconSize),
                                      name);
        item->setData(name);
        item->setSizeHint(QSize(SIZES.mangasourceItemWidth, SIZES.mangasourceItemHeight));
        model->appendRow(item);
    }

    updateMangaSourceSelection();
    adjustSourcesHeight();
}

void HomeWidget::updateMangaSourceSelection()
{
    auto model = dynamic_cast<QStandardItemModel *>(ui->listViewSources->model());
    for (int i = 0; i < model->rowCount(); i++)
    {
        QStandardItem *item = model->item(i);
        auto f = item->font();
        f.setUnderline(item->data().toString() == currentSource);
        item->setFont(f);
    }
}

void HomeWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    adjustSourcesHeight();
}

// One row of sources, two when they don't fit into one (more are scrolled to).
void HomeWidget::adjustSourcesHeight()
{
    auto view = ui->listViewSources;
    int itemWidth = SIZES.mangasourceItemWidth + view->spacing();
    int perRow = qMax(1, (view->width() - 2 * view->frameWidth()) / itemWidth);
    int rows = view->model()->rowCount() > perRow ? 2 : 1;
    view->setFixedHeight(SIZES.listSourcesHeight + (rows - 1) * (SIZES.mangasourceItemHeight + view->spacing()));
}

void HomeWidget::on_listViewSources_clicked(const QModelIndex &index)
{
    emit mangaSourceClicked(index.data(Qt::UserRole + 1).toString());
}

void HomeWidget::currentSourceChanged(const QString &source)
{
    currentSource = source;
    updateMangaSourceSelection();
    showMangaList();
}

MangaList &HomeWidget::currentList()
{
    return mangaLists->list(currentSource);
}

void HomeWidget::mangaListChanged(const QString &source)
{
    if (source == currentSource)
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
    if (currentSource.isEmpty())
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

    if (currentSource.isEmpty())
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
    auto source = sources->source(currentSource);
    if (!source || idx < 0 || idx >= (filteractive ? filteredMangaUrls.size() : currentList().urls.size()))
        return;

    QString mangaUrl = filteractive ? filteredMangaUrls[idx] : currentList().urls[idx];

    // the lists keep the urls without the base (they have megabytes)
    if (!currentList().absoluteUrls)
        mangaUrl.prepend(source->baseUrl);

    QString mangatitle = filteractive ? filteredMangaTitles[idx] : currentList().titles[idx];

    emit mangaClicked(mangaUrl, mangatitle);
}
