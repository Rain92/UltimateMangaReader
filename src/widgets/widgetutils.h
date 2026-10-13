#pragma once

#include <QAbstractScrollArea>

// Kinetic scrolling by dragging with the finger (or mouse).
void activateScroller(QAbstractScrollArea* pArea);

// The device pixel ratio the main window is painted with. It can differ from qApp->devicePixelRatio(), on
// Wayland with fractional scaling that is the ratio of the screens rounded up (2 instead of 1.25).
// Thread safe.
qreal windowPixelRatio();
void setWindowPixelRatio(qreal ratio);
