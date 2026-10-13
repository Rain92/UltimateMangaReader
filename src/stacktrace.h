#ifndef STACKTRACE_H
#define STACKTRACE_H

// Prints a backtrace to stderr when the app crashes (SIGSEGV, SIGILL, SIGFPE, SIGABRT, SIGBUS) or gets SIGUSR2,
// then lets the signal terminate the app as usual. Returns false if a handler couldn't be installed.
bool registerBacktraceHandlers();

#endif  // STACKTRACE_H
