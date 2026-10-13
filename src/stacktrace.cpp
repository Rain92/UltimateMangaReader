#include "stacktrace.h"

#include <execinfo.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <cstdio>

namespace
{
const int catchSignals[] = {SIGSEGV, SIGILL, SIGFPE, SIGABRT, SIGBUS, SIGUSR2};

void writeStderr(const char *str)
{
    ssize_t r = write(STDERR_FILENO, str, strlen(str));
    (void)r;
}

// only async signal safe functions in here
void doBacktrace(int signo)
{
    char number[12];
    int pos = sizeof(number) - 1;
    number[pos] = '\0';
    for (int n = signo; n > 0 || pos == (int)sizeof(number) - 1; n /= 10)
        number[--pos] = '0' + n % 10;

    writeStderr("Received signal ");
    writeStderr(number + pos);
    writeStderr(", backtrace:\n");

    void *frames[50];
    int size = backtrace(frames, 50);
    backtrace_symbols_fd(frames, size, STDERR_FILENO);

    // the handler was reset (SA_RESETHAND): the signal does what it would have done without it, e.g. a core dump
    raise(signo);
}
}  // namespace

bool registerBacktraceHandlers()
{
    // backtrace() loads libgcc the first time, which isn't safe in a signal handler
    void *frames[1];
    backtrace(frames, 1);

    struct sigaction action = {};
    action.sa_handler = doBacktrace;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESETHAND;

    bool result = true;
    for (int signo : catchSignals)
    {
        if (sigaction(signo, &action, nullptr) != 0)
        {
            fprintf(stderr, "Failed to install the handler for signal %d\n", signo);
            result = false;
        }
    }

    return result;
}
