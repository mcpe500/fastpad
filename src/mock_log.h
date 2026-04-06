#ifndef MOCK_LOG_H
#define MOCK_LOG_H
#include <stdio.h>
#define log_info(...) printf("[INFO] ")
#define log_error(...) printf("[ERROR] ")
#define log_action(...) printf("[ACTION] ")
#endif
