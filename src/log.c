#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <windows.h>

#ifdef DEV_BUILD
static FILE *log_file = NULL;

void init_logging() {
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    
    // Remove filename to get the directory
    char *last_slash = strrchr(exe_path, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }
    
    char log_path[MAX_PATH];
    snprintf(log_path, sizeof(log_path), "%s\\fastpad_dev.log", exe_path);
    
    log_file = fopen(log_path, "w");
    if (log_file) {
        fprintf(log_file, "--- FastPad Dev Log Started ---\\n");
        fprintf(log_file, "Executable path: %s\\n", exe_path);
        fprintf(log_file, "Log file location: %s\\n", log_path);
    }
}

void close_logging() {
    if (log_file) {
        fprintf(log_file, "--- FastPad Dev Log Ended ---\\n");
        fclose(log_file);
        log_file = NULL;
    }
}

static void write_log(const char *level, const char *fmt, va_list args) {
    if (!log_file) return;
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
    
    fprintf(log_file, "[%s] [%s] ", time_str, level);
    vfprintf(log_file, fmt, args);
    fprintf(log_file, "\\n");
    fflush(log_file);
}

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log("INFO", fmt, args);
    va_end(args);
}

void log_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log("WARN", fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log("ERROR", fmt, args);
    va_end(args);
}

void log_action(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log("ACTION", fmt, args);
    va_end(args);
}
#endif
