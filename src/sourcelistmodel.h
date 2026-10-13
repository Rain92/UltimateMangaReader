#pragma once

#include <QAbstractListModel>
#include <QSet>

#include "sourceregistry.h"

// The sources of the registry with a check box each, for the dialogs. It follows the changes of the registry.
class SourceListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Mode
    {
        // all sources, the check enables the source
        EnableSources,
        // the enabled sources, checked are the selected ones (e.g. to update), all at first
        SelectSources,
    };
    enum Roles
    {
        NameRole = Qt::UserRole + 1,
    };

    SourceListModel(SourceRegistry *sources, Mode mode, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    bool isChecked(const QString &name) const;
    void setChecked(const QString &name, bool checked);
    // SelectSources: checks all of them.
    void selectAll();
    // In the order of the list.
    QStringList checkedNames() const;

private:
    SourceRegistry *sources;
    Mode mode;
    QStringList names;
    // SelectSources: the ones that were unchecked, a new source is checked
    QSet<QString> unselected;

    QStringList currentNames() const;
    void sourcesChanged();
};
