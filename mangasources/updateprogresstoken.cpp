#include "updateprogresstoken.h"

UpdateProgressToken::UpdateProgressToken()
    : QObject(), currentSourceName(), sourcesProgress(), canceled(false)
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

void UpdateProgressToken::sendFinished()
{
    emit updateFinished();
}
