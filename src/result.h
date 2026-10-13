#pragma once

#include <QString>

#include <expected>

// What went wrong: the message is for the user, the code for the code that decides what to do about it.
struct Error
{
    enum Code
    {
        // anything else
        Failed,
        // no connection, no answer
        Network,
        // the download took too long
        Timeout,
        Cancelled,
        // the server answered with an error status (httpStatus)
        Http,
        // the response isn't what the source expects (e.g. the site changed)
        Parse,
        // e.g. a chapter without pages, a manga that isn't there anymore
        NotFound,
        // a file couldn't be written or read
        Storage,
    };

    Code code = Failed;
    QString message;
    // of the response that failed, 0 without one
    int httpStatus = 0;

    bool isCancelled() const { return code == Cancelled; }
};

// A value, or the error why there is none.
template <typename T = void>
using Result = std::expected<T, Error>;

inline std::unexpected<Error> Err(Error error)
{
    return std::unexpected(std::move(error));
}

inline std::unexpected<Error> Err(Error::Code code, const QString &message)
{
    return std::unexpected(Error{code, message, 0});
}

inline std::unexpected<Error> Err(const QString &message)
{
    return Err(Error::Failed, message);
}
