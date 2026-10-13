#pragma once

#include <QWidget>

#include "sourcelistmodel.h"

// The sources of a SourceListModel as check boxes, for the dialogs. It follows the model.
class SourceCheckList : public QWidget
{
    Q_OBJECT

public:
    SourceCheckList(SourceListModel *model, QWidget *parent = nullptr);

private:
    SourceListModel *model;

    void rebuild();
    void updateChecks();
};
