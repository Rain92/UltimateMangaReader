#pragma once

#include <QByteArray>
#include <QVariant>

#include "result.h"

// Reads YAML (its first document) into QVariants: mappings become QVariantMaps, sequences QVariantLists and
// scalars QStrings, except for the plain (unquoted) integers (qlonglong) and true and false (bool). Nothing else
// is converted ("no", "1.10", "null" stay strings), the readers check what they expect. A key that is there twice
// is an error. An empty document is an empty map. name is for the messages ("<name>:<line>: <problem>").
Result<QVariant> readYaml(const QByteArray &yaml, const QString &name);
