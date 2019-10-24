#include "logging.h"

#include <stdio.h>
#include <stdarg.h>

static int log_level = LOG_WARNING; // Default logging level
static FILE *log_file = NULL;

void SetLogLevel(int level) {
    log_level = level;
}

void SetLogStream(FILE* stream) {
    log_file = stream;
}

void Log(int level, const char* message) {
    if (level < log_level)
        return;
    if (log_file == NULL)
        log_file = stderr;
    fprintf(log_file, "%s\n", message);
}

void Logf(int level, const char* format, ...) {
    if (level < log_level)
        return;
    if (log_file == NULL)
        log_file = stderr;
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fputc('\n', log_file);
}
