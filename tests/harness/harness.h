#pragma once

#include <QSharedPointer>
#include <QTextStream>

class MangaRepository;
class MangaSourceBase;
class NetworkManager;
struct MangaList;

// the output that is compared (the log of the app goes to stderr)
extern QTextStream out;

// What the source makes of the mangas in the picks file (title<TAB>url per line): the info, the chapters, the
// page lists, image links and the first image. Returns the number of failures.
int captureMangas(MangaRepository &repository, MangaSourceBase *source, const QString &picksFile);
// The update of the manga list of the source, the list goes to result. Returns the number of failures.
int captureList(MangaSourceBase *source, MangaList *result = nullptr);

int jobTests(NetworkManager &nm, const QString &server);
// Opens the manga, turns pages, makes it a favorite and downloads a chapter, through the core like the gui.
int flowTests(const QString &source, const QString &title, const QString &url);
// Updates the manga list, cancels an update and updates it again, through the core like the gui.
int listFlowTests(const QString &source);
int storageTests(const QString &server);
// The time of what reading asks of the source for the mangas of the picks file (info, page lists, resolved pages,
// images): the median of rounds, of imageRounds for the images.
int benchReading(MangaRepository &repository, MangaSourceBase *source, const QString &picksFile, int rounds,
                 int imageRounds);
// The Lua engine, its libraries and the sources of scripts, against tests/harness/testserver.py.
int scriptTests(const QString &server);
