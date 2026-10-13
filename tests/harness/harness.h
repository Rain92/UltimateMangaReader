#pragma once

#include <QSharedPointer>
#include <QTextStream>

class AbstractMangaSource;
class NetworkManager;

// the output that is compared (the log of the app goes to stderr)
extern QTextStream out;

AbstractMangaSource *findSource(const QList<QSharedPointer<AbstractMangaSource>> &sources, const QString &name);

// What the source makes of the mangas in the picks file (title<TAB>url per line): the info, the chapters, the
// page lists, image links and the first image. Returns the number of failures.
int captureMangas(AbstractMangaSource *source, const QString &picksFile);
// The update of the manga list of the source. Returns the number of failures.
int captureList(AbstractMangaSource *source);

int jobTests(NetworkManager &nm, const QString &server);
int storageTests(const QString &server);
