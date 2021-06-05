#ifndef STACKTRACE_H
#define STACKTRACE_H

#include <execinfo.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <exception>
#include <iostream>

void doBacktrace(int signo);

const struct catchSignals_t
{
    int signo;
    const char *name;
} catchSignals[] = {{SIGSEGV, "SIGSEGV"}, {SIGILL, "SIGILL"}, {SIGFPE, "SIGFPE"},
                    {SIGABRT, "SIGABRT"}, {SIGBUS, "SIGBUS"}, {SIGUSR2, "SIGUSR2"}};

bool registerBacktraceHandlers()
{
    bool result = true;

    for (auto i : catchSignals)
    {
        result = result && (signal(i.signo, doBacktrace) != SIG_ERR);

        if (!result)
            std::cerr << "Failed to install signal:" << i.name;
    }

    return result;
};

void doBacktrace(int signo)
{
    std::cerr << "Received signal " << signo << std::endl;
    void *array[50];
    int size = backtrace(array, 50);

    std::cerr << " backtrace returned " << size << " frames\n\n";

    char **messages = backtrace_symbols(array, size);

    for (int i = 0; i < size && messages != NULL; ++i)
    {
        std::cerr << "[bt]: (" << i << ") " << messages[i] << std::endl;
    }
    std::cerr << std::endl;

    free(messages);
    exit(0);
}

#endif  // STACKTRACE_H
