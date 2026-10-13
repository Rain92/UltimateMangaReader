#pragma once

#include <QAbstractScrollArea>
#include <QIcon>

// Kinetic scrolling by dragging with the finger (or mouse).
void activateScroller(QAbstractScrollArea* pArea);

// The device pixel ratio the main window is painted with. It can differ from qApp->devicePixelRatio(), on
// Wayland with fractional scaling that is the ratio of the screens rounded up (2 instead of 1.25).
// Thread safe.
qreal windowPixelRatio();
void setWindowPixelRatio(qreal ratio);

// The logo of the source from the resources, else one with its initials (e.g. a source that was added).
QIcon sourceLogo(const QString &name, int size);
