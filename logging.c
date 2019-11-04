#include "logging.h"

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

static int log_level = LOG_WARNING; // Default logging level
static int log_color = 1; // Log with color
static FILE *log_file = NULL;
static const struct {
    const char *colored;
    const char *normal;
} LogPrefix[] = {
    {"\e[0m[D]\e[0m", "[D]"},
    {"\e[36m[I]\e[0m", "[I]"},
    {"\e[33m[W]\e[0m", "[W]"},
    {"\e[31m[E]\e[0m", "[E]"},
    {"\e[35m[F]\e[0m", "[F]"}
};

void SetLogLevel(int level) {
    log_level = level;
}

void SetLogStream(FILE* stream) {
    log_file = stream;
    log_color = isatty(fileno(stream));
}

static inline const char *GetLogPrefix(int level) {
    if (log_color) {
        return LogPrefix[level].colored;
    } else {
        return LogPrefix[level].normal;
    }
}

void Log(int level, const char* message) {
    if (level < log_level)
        return;
    if (log_file == NULL)
        SetLogStream(stderr);
    fprintf(log_file, "%s %s\n", GetLogPrefix(level), message);
}

void Logf(int level, const char* format, ...) {
    if (level < log_level)
        return;
    if (log_file == NULL)
        SetLogStream(stderr);
    fprintf(log_file, "%s ", GetLogPrefix(level));
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fputc('\n', log_file);
}
