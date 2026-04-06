#ifndef LOG_H
#define LOG_H

#ifdef DEV_BUILD
    void init_logging();
    void close_logging();
    void log_info(const char *fmt, ...);
    void log_warn(const char *fmt, ...);
    void log_error(const char *fmt, ...);
    void log_action(const char *fmt, ...);
#else
    #define log_info(...) ((void)0)
    #define log_warn(...) ((void)0)
    #define log_error(...) ((void)0)
    #define log_action(...) ((void)0)
#endif

#endif
