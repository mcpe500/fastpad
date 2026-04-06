#include <stdio.h>
#include <stdarg.h>

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[INFO] ");
    vprintf(fmt, args);
    printf("\\n");
    va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[ERROR] ");
    vprintf(fmt, args);
    printf("\\n");
    va_end(args);
}

void log_action(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[ACTION] ");
    vprintf(fmt, args);
    printf("\\n");
    va_end(args);
}
