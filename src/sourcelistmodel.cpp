#include "sourcelistmodel.h"

SourceListModel::SourceListModel(SourceRegistry *sources, Mode mode, QObject *parent)
    : QAbstractListModel(parent), sources(sources), mode(mode), names(), unselected()
{
    names = currentNames();
    connect(sources, &SourceRegistry::sourcesChanged, this, &SourceListModel::sourcesChanged);
}

QStringList SourceListModel::currentNames() const
{
    return mode == EnableSources ? sources->names() : sources->enabledNames();
}

void SourceListModel::sourcesChanged()
{
    auto now = currentNames();
    if (now == names)
    {
        // e.g. one was enabled in the list of all sources
        if (!names.isEmpty())
            emit dataChanged(index(0), index(names.size() - 1), {Qt::CheckStateRole});
        return;
    }

    beginResetModel();
    names = now;
    endResetModel();
}

int SourceListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : names.size();
}

QVariant SourceListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= names.size())
        return {};

    const auto &name = names[index.row()];
    switch (role)
    {
        case Qt::DisplayRole:
        case NameRole:
            return name;
        case Qt::CheckStateRole:
            return isChecked(name) ? Qt::Checked : Qt::Unchecked;
        default:
            return {};
    }
}

bool SourceListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::CheckStateRole || !index.isValid() || index.row() >= names.size())
        return false;

    setChecked(names[index.row()], value.toInt() == Qt::Checked);
    return true;
}

bool SourceListModel::isChecked(const QString &name) const
{
    return mode == EnableSources ? sources->isEnabled(name) : !unselected.contains(name);
}

void SourceListModel::setChecked(const QString &name, bool checked)
{
    if (mode == EnableSources)
    {
        // the registry signals the change
        sources->setEnabled(name, checked);
        return;
    }

    if (checked)
        unselected.remove(name);
    else
        unselected.insert(name);

    int row = names.indexOf(name);
    if (row >= 0)
        emit dataChanged(index(row), index(row), {Qt::CheckStateRole});
}

void SourceListModel::selectAll()
{
    if (mode != SelectSources || unselected.isEmpty())
        return;

    unselected.clear();
    emit dataChanged(index(0), index(names.size() - 1), {Qt::CheckStateRole});
}

QStringList SourceListModel::checkedNames() const
{
    QStringList checked;
    for (const auto &name : names)
        if (isChecked(name))
            checked.append(name);
    return checked;
}
