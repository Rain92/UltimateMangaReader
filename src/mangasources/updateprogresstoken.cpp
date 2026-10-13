#include "updateprogresstoken.h"

UpdateProgressToken::UpdateProgressToken()
    : QObject(), currentSourceName(), sourcesProgress(), canceled(false), resumeStates()
{
}

void UpdateProgressToken::sendProgress(int p)
{
    sourcesProgress[currentSourceName] = p;
    emit updateProgress();
}

void UpdateProgressToken::sendError(const QString& message)
{
    emit updateError(message);
}

void UpdateProgressToken::sendFinished(const QString& message)
{
    emit updateFinished(message);
}

void UpdateProgressToken::clearResumeState()
{
    resumeStates.remove(currentSourceName);
}
