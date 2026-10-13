#pragma once

#include <QString>
#include <QStringList>

// The HTTP responses of a test, recorded from the sites into a directory (index.json and the bodies) and
// answered from there in later runs, so the output doesn't change with the sites. Requests are matched by
// method and url (without the fragment). All requests of the app go through NetworkAccessManager, which
// lets the fixtures answer them.
namespace Fixtures
{
enum class Mode
{
    Live,
    Record,
    Replay
};

// Answers the app's requests from now on (Replay) or records them (Record).
bool install(Mode mode, const QString &dir);
// Writes the recorded responses, replacing the ones that were in the directory.
bool save();
// The requests that the fixtures didn't have an answer for (Replay).
QStringList missing();
}  // namespace Fixtures
