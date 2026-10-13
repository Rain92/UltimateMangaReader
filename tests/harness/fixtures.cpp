#include "fixtures.h"

#include <QCryptographicHash>
#include <QDir>
#include <QHttpHeaders>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>

#include "networkmanager.h"

namespace
{
struct Response
{
    QString finalUrl;
    int status = 0;
    QByteArray reason;
    int error = QNetworkReply::NoError;
    QString errorString;
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;
    // the app stopped it (e.g. after it read the start of an image), the body is what arrived until then
    bool partial = false;
};

Fixtures::Mode mode = Fixtures::Mode::Live;
QString directory;
QHash<QString, Response> responses;
QStringList missingRequests;

QString requestKey(QNetworkAccessManager::Operation op, const QNetworkRequest &request)
{
    QString method;
    switch (op)
    {
        case QNetworkAccessManager::HeadOperation:
            method = "HEAD";
            break;
        case QNetworkAccessManager::GetOperation:
            method = "GET";
            break;
        case QNetworkAccessManager::PutOperation:
            method = "PUT";
            break;
        case QNetworkAccessManager::PostOperation:
            method = "POST";
            break;
        case QNetworkAccessManager::DeleteOperation:
            method = "DELETE";
            break;
        default:
            method = QString::fromLatin1(request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray());
    }
    return method + " " + request.url().toString(QUrl::RemoveFragment | QUrl::FullyEncoded);
}

void store(const QString &key, const Response &response)
{
    // the start of a file doesn't replace the whole file
    auto it = responses.constFind(key);
    if (it != responses.constEnd() && response.partial && !it->partial)
        return;
    responses.insert(key, response);
}

QHttpHeaders toHttpHeaders(const QList<QPair<QByteArray, QByteArray>> &pairs)
{
    QHttpHeaders headers;
    for (const auto &[name, value] : pairs)
        headers.append(QLatin1StringView(name), QLatin1StringView(value));
    return headers;
}

// A reply that hands out the data it got (from the network or the fixtures).
class BufferedReply : public QNetworkReply
{
public:
    BufferedReply(QNetworkAccessManager *manager, QNetworkAccessManager::Operation op,
                  const QNetworkRequest &request)
        : QNetworkReply(manager), offset(0)
    {
        setRequest(request);
        setOperation(op);
        setUrl(request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    qint64 bytesAvailable() const override { return buffer.size() - offset + QNetworkReply::bytesAvailable(); }

protected:
    QByteArray buffer;
    qint64 offset;

    qint64 readData(char *data, qint64 maxSize) override
    {
        qint64 n = qMin(maxSize, buffer.size() - offset);
        memcpy(data, buffer.constData() + offset, n);
        offset += n;
        if (offset == buffer.size())
        {
            buffer.clear();
            offset = 0;
        }
        return n;
    }

    void applyMetaData(const Response &response)
    {
        if (!response.finalUrl.isEmpty())
            setUrl(QUrl(response.finalUrl));
        if (response.status != 0)
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.status);
        if (!response.reason.isEmpty())
            setAttribute(QNetworkRequest::HttpReasonPhraseAttribute, response.reason);
        setHeaders(toHttpHeaders(response.headers));
    }
};

// Passes a reply of the network through and records it.
class RecordingReply : public BufferedReply
{
public:
    RecordingReply(QNetworkAccessManager *manager, QNetworkAccessManager::Operation op,
                   const QNetworkRequest &request, const QString &key, QNetworkReply *real)
        : BufferedReply(manager, op, request), key(key), real(real), aborted(false), stored(false)
    {
        // like the app (DownloadJobBase::prepareReply), the reply of the network is the one that gets them
        real->ignoreSslErrors();

        connect(real, &QNetworkReply::metaDataChanged, this,
                [this]()
                {
                    takeMetaData();
                    emit metaDataChanged();
                });
        connect(real, &QNetworkReply::readyRead, this,
                [this]()
                {
                    takeData();
                    emit readyRead();
                });
        connect(real, &QNetworkReply::downloadProgress, this, &QNetworkReply::downloadProgress);
        connect(real, &QNetworkReply::errorOccurred, this,
                [this](NetworkError code)
                {
                    setError(code, this->real->errorString());
                    emit errorOccurred(code);
                });
        connect(real, &QNetworkReply::finished, this,
                [this]()
                {
                    takeMetaData();
                    if (this->real->bytesAvailable() > 0)
                    {
                        takeData();
                        emit readyRead();
                    }
                    response.error = this->real->error();
                    response.errorString = this->real->errorString();
                    response.partial = aborted;
                    storeOnce();
                    setFinished(true);
                    emit finished();
                });
    }

    ~RecordingReply() override
    {
        // deleted while it was running: the start of a file
        if (!stored)
        {
            response.partial = true;
            response.error = OperationCanceledError;
            response.errorString = "Operation canceled";
            storeOnce();
        }

        // it stays a child of the manager: the manager's destructor deletes all replies it finds among its
        // children (also the children of the children), it would delete it a second time
        if (real)
        {
            real->disconnect(this);
            real->abort();
            real->deleteLater();
        }
    }

    void abort() override
    {
        aborted = true;
        if (real)
            real->abort();
    }

private:
    QString key;
    QPointer<QNetworkReply> real;
    Response response;
    bool aborted;
    bool stored;

    void takeMetaData()
    {
        response.finalUrl = real->url().toString(QUrl::FullyEncoded);
        response.status = real->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        response.reason = real->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toByteArray();
        response.headers.clear();
        const auto headers = real->headers();
        for (qsizetype i = 0; i < headers.size(); i++)
            response.headers.append({headers.nameAt(i).toString().toLatin1(), headers.valueAt(i).toByteArray()});
        applyMetaData(response);
    }

    void takeData()
    {
        auto data = real->readAll();
        response.body += data;
        buffer += data;
    }

    void storeOnce()
    {
        if (stored)
            return;
        stored = true;
        store(key, response);
    }
};

// Answers a request with a recorded response.
class ReplayReply : public BufferedReply
{
public:
    // response is null if there is none for the request
    ReplayReply(QNetworkAccessManager *manager, QNetworkAccessManager::Operation op,
                const QNetworkRequest &request, const Response *response)
        : BufferedReply(manager, op, request), done(false)
    {
        if (response)
            this->response = *response;
        else
            this->response.error = ProtocolUnknownError;
        missing = !response;

        // like the network: later, from the event loop
        QMetaObject::invokeMethod(this, [this]() { deliver(); }, Qt::QueuedConnection);
    }

    void abort() override
    {
        if (done)
            return;
        setError(OperationCanceledError, "Operation canceled");
        emit errorOccurred(OperationCanceledError);
        finish();
    }

private:
    Response response;
    bool missing;
    bool done;

    void deliver()
    {
        if (done)
            return;

        if (missing)
        {
            setError(ProtocolUnknownError, "Not in the fixtures: " + url().toString());
            emit errorOccurred(ProtocolUnknownError);
            finish();
            return;
        }

        applyMetaData(response);
        emit metaDataChanged();
        if (done)
            return;

        if (!response.body.isEmpty())
        {
            buffer = response.body;
            emit downloadProgress(buffer.size(), buffer.size());
            emit readyRead();
            // the reader stopped it
            if (done)
                return;
        }

        if (response.error != NoError)
        {
            auto code = static_cast<NetworkError>(response.error);
            setError(code, response.errorString);
            emit errorOccurred(code);
        }
        finish();
    }

    void finish()
    {
        done = true;
        setFinished(true);
        emit finished();
    }
};

QString bodiesDir()
{
    return directory + "/bodies/";
}

bool load()
{
    responses.clear();
    QFile file(directory + "/index.json");
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "No fixtures in" << directory;
        return false;
    }

    const auto index = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = index.begin(); it != index.end(); ++it)
    {
        const auto r = it.value().toObject();
        Response response;
        response.finalUrl = r["finalUrl"].toString();
        response.status = r["status"].toInt();
        response.reason = r["reason"].toString().toLatin1();
        response.error = r["error"].toInt();
        response.errorString = r["errorString"].toString();
        for (const auto &header : r["headers"].toArray())
            response.headers.append(
                {header[0].toString().toLatin1(), header[1].toString().toLatin1()});
        response.partial = r["partial"].toBool();
        if (r.contains("body"))
        {
            QFile body(bodiesDir() + r["body"].toString());
            if (!body.open(QIODevice::ReadOnly))
            {
                qWarning() << "Missing body" << body.fileName();
                return false;
            }
            response.body = body.readAll();
        }
        responses.insert(it.key(), response);
    }
    return true;
}
}  // namespace

namespace Fixtures
{
bool install(Mode newMode, const QString &dir)
{
    mode = newMode;
    directory = dir;
    responses.clear();
    missingRequests.clear();

    if (mode == Mode::Live)
    {
        NetworkAccessManager::interceptor = nullptr;
        return true;
    }

    if (mode == Mode::Replay && !load())
        return false;

    NetworkAccessManager::interceptor =
        [](QNetworkAccessManager *manager, QNetworkAccessManager::Operation op, const QNetworkRequest &request,
           const std::function<QNetworkReply *()> &send) -> QNetworkReply *
    {
        auto key = requestKey(op, request);
        if (mode == Mode::Record)
            return new RecordingReply(manager, op, request, key, send());

        auto it = responses.constFind(key);
        if (it == responses.constEnd())
        {
            qWarning().noquote() << "Not in the fixtures:" << key;
            missingRequests.append(key);
            return new ReplayReply(manager, op, request, nullptr);
        }
        return new ReplayReply(manager, op, request, &*it);
    };
    return true;
}

bool save()
{
    if (mode != Mode::Record)
        return true;

    QDir(bodiesDir()).removeRecursively();
    QDir().mkpath(bodiesDir());

    QJsonObject index;
    for (auto it = responses.cbegin(); it != responses.cend(); ++it)
    {
        const auto &response = it.value();
        QJsonObject r;
        if (response.finalUrl != it.key().section(' ', 1))
            r["finalUrl"] = response.finalUrl;
        if (response.status != 0)
            r["status"] = response.status;
        if (!response.reason.isEmpty())
            r["reason"] = QString::fromLatin1(response.reason);
        if (response.error != QNetworkReply::NoError)
        {
            r["error"] = response.error;
            r["errorString"] = response.errorString;
        }
        QJsonArray headers;
        for (const auto &[name, value] : response.headers)
            headers.append(QJsonArray{QString::fromLatin1(name), QString::fromLatin1(value)});
        r["headers"] = headers;
        if (response.partial)
            r["partial"] = true;
        if (!response.body.isEmpty())
        {
            auto hash = QString::fromLatin1(QCryptographicHash::hash(response.body, QCryptographicHash::Sha1).toHex());
            QFile body(bodiesDir() + hash);
            if (!body.exists() && (!body.open(QIODevice::WriteOnly) || body.write(response.body) != response.body.size()))
                return false;
            r["body"] = hash;
        }
        index[it.key()] = r;
    }

    QSaveFile file(directory + "/index.json");
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(index).toJson(QJsonDocument::Indented));
    return file.commit();
}

QStringList missing()
{
    return missingRequests;
}
}  // namespace Fixtures
