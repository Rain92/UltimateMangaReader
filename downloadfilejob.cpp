#include "downloadfilejob.h"

#include "defines.h"
#include "downloadmanager.h"

DownloadFileJob::DownloadFileJob(QObject *parent,
                                 QNetworkAccessManager *networkManager,
                                 const QString &url,
                                 const QString &localFilePath)
    : DownloadJobBase(parent, networkManager, url), filepath(localFilePath)
{
}

void DownloadFileJob::start()
{
    QString dirname = QFileInfo(filepath).path();
    QDir().mkpath(dirname);

    file.setFileName(filepath + ".part");

    if (QFile::exists(filepath))
    {
        isCompleted = true;
    }
    else
    {
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            QNetworkRequest request(url);
            reply.reset(networkManager->get(request));
            reply->setParent(this);

            QObject::connect(reply.get(), SIGNAL(readyRead()), this,
                             SLOT(downloadFileReadyRead()));
            QObject::connect(reply.get(), SIGNAL(finished()), this,
                             SLOT(downloadFileFinished()));
            QObject::connect(reply.get(),
                             SIGNAL(error(QNetworkReply::NetworkError)), this,
                             SLOT(onError(QNetworkReply::NetworkError)));
            QObject::connect(reply.get(),
                             SIGNAL(sslErrors(const QList<QSslError> &)), this,
                             SLOT(onSslErrors(const QList<QSslError> &)));
        }
        else
        {
            errorString = "Can't create file.";
            emit downloadError();
        }
    }
}

DownloadFileJob::~DownloadFileJob() {}

void DownloadFileJob::restart()
{
    isCompleted = false;
    errorString = "";
    reply.reset();
    start();
}

void DownloadFileJob::downloadFileReadyRead() { file.write(reply->readAll()); }

void DownloadFileJob::downloadFileFinished()
{
    if (file.isOpen())
    {
        file.flush();
        file.close();
    }

    QUrl redirect =
        reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirect.isValid() && reply->url() != redirect)
    {
        this->url = redirect.toString();
        this->restart();
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        file.remove();
        onError(QNetworkReply::NetworkError());
    }
    else
    {
        isCompleted = true;

        file.rename(filepath);

        emit completed();
    }
}

void DownloadFileJob::onError(QNetworkReply::NetworkError)
{
    if (file.isOpen())
    {
        file.flush();
        file.close();
    }

    file.remove();

    errorString = reply->errorString();

    qDebug() << errorString;

    emit downloadError();
}

bool DownloadFileJob::await(int timeout)
{
    if (isCompleted)
        return true;

    QEventLoop loop;
    connect(this, SIGNAL(completed()), &loop, SLOT(quit()));
    connect(this, SIGNAL(downloadError()), &loop, SLOT(quit()));

    QTimer timer;
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(timeout);

    if (errorString != "")
        return false;

    if (isCompleted)
        return true;

    loop.exec();

    return isCompleted;
}
