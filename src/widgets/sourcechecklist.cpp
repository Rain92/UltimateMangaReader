#include "sourcechecklist.h"

#include <QCheckBox>
#include <QVBoxLayout>

SourceCheckList::SourceCheckList(SourceListModel *model, QWidget *parent) : QWidget(parent), model(model)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    connect(model, &QAbstractItemModel::modelReset, this, &SourceCheckList::rebuild);
    connect(model, &QAbstractItemModel::dataChanged, this, &SourceCheckList::updateChecks);
    rebuild();
}

void SourceCheckList::rebuild()
{
    // a reset can come from a click on one of them
    for (auto checkbox : findChildren<QCheckBox *>(Qt::FindDirectChildrenOnly))
    {
        layout()->removeWidget(checkbox);
        checkbox->deleteLater();
    }

    for (int row = 0; row < model->rowCount(); row++)
    {
        auto name = model->index(row).data(SourceListModel::NameRole).toString();
        auto checkbox = new QCheckBox(name, this);
        // no focus frame on the first one, it's for touch
        checkbox->setFocusPolicy(Qt::NoFocus);
        checkbox->setChecked(model->isChecked(name));
        connect(checkbox, &QCheckBox::clicked, this,
                [this, name](bool checked) { model->setChecked(name, checked); });
        layout()->addWidget(checkbox);
    }
}

void SourceCheckList::updateChecks()
{
    for (auto checkbox : findChildren<QCheckBox *>(Qt::FindDirectChildrenOnly))
        checkbox->setChecked(model->isChecked(checkbox->text()));
}
