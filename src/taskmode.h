#ifndef TASKMODE_H
#define TASKMODE_H

#include "core_types.h"
#include <windows.h>

#define MAX_TASKS 200
#define MAX_TASK_TEXT 512
#define MAX_TASK_LISTS 20
#define MAX_TASK_LIST_NAME 64

typedef enum {
    TASK_PENDING,
    TASK_DONE,
    TASK_CANCELLED
} TaskStatus;

typedef struct {
    char id[32];
    char text[MAX_TASK_TEXT];
    TaskStatus status;
    int priority;  // 0=low, 1=medium, 2=high
    FILETIME created;
    FILETIME completed;
    char list_name[MAX_TASK_LIST_NAME];
} Task;

typedef struct {
    Task tasks[MAX_TASKS];
    int count;
    char data_dir[MAX_PATH];
    char active_list[MAX_TASK_LIST_NAME];
} TaskManager;

bool task_init(TaskManager *mgr, HWND parent);
void task_free(TaskManager *mgr);
bool task_save(TaskManager *mgr);
bool task_load(TaskManager *mgr);
bool task_add(TaskManager *mgr, const char *text, const char *list_name, int priority);
bool task_remove(TaskManager *mgr, int index);
bool task_update(TaskManager *mgr, int index, const char *text);
bool task_set_status(TaskManager *mgr, int index, TaskStatus status);
bool task_set_priority(TaskManager *mgr, int index, int priority);
int task_get_count(TaskManager *mgr);
int task_get_pending_count(TaskManager *mgr);
int task_find_by_id(TaskManager *mgr, const char *id);
bool task_move(TaskManager *mgr, int from_index, int to_index);
bool task_list_create(TaskManager *mgr, const char *list_name);
bool task_list_delete(TaskManager *mgr, const char *list_name);
int task_list_get_count(TaskManager *mgr);
const char* task_list_get_name(TaskManager *mgr, int index);

#endif // TASKMODE_H