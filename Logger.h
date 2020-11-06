#ifndef LOGGER_H
#define LOGGER_H

enum LOG_LEVEL {
    DEBUG,
    WARNING,
    ERROR,
    CRITICAL
};

int InitializeLog();

void SetLogLevel(LOG_LEVEL level);
void Log(LOG_LEVEL level, const char *program, const char *function,
        int line, const char *message);
void ExitLog();

#endif
