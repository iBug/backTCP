#ifndef __LOGGING_H
#define __LOGGING_H

#include <stdio.h>

#define LOG_DEBUG 0
#define LOG_INFO 1
#define LOG_WARN 2
#define LOG_WARNING 2
#define LOG_ERROR 3
#define LOG_FATAL 4
#define LOG_CRITICAL 4

void SetLogLevel(int level);
void SetLogStream(FILE* stream);

void Log(int level, const char* message);
void Logf(int level, const char* format, ...);

#endif // __LOGGING_H
