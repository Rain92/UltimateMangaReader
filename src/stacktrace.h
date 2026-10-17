#pragma once

// Prints a backtrace to stderr when the app crashes (SIGSEGV, SIGILL, SIGFPE, SIGABRT, SIGBUS) or gets SIGUSR2,
// then lets the signal terminate the app as usual. Returns false if a handler couldn't be installed.
bool registerBacktraceHandlers();

// Called by the handlers before the signal terminates the app, e.g. to undo a change to the system. It may only
// call async signal safe functions.
void setCrashCleanup(void (*cleanup)());
