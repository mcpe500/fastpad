#include "taskmode.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

static bool task_ensure_dir(TaskManager *mgr) {
    if (mgr->data_dir[0] == '\0') return false;
    return CreateDirectoryA(mgr->data_dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool task_init(TaskManager *mgr, HWND parent) {
    (void)parent; // unused
    memset(mgr, 0, sizeof(TaskManager));
    
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        snprintf(mgr->data_dir, MAX_PATH, "%s\\FastPad\\tasks", appdata);
    } else {
        return false;
    }
    
    strncpy(mgr->active_list, "Inbox", MAX_TASK_LIST_NAME - 1);
    
    task_ensure_dir(mgr);
    task_load(mgr);
    return true;
}

void task_free(TaskManager *mgr) {
    task_save(mgr);
}

bool task_save(TaskManager *mgr) {
    if (mgr->data_dir[0] == '\0') return false;
    
    char filepath[MAX_PATH];
    snprintf(filepath, MAX_PATH, "%s\\tasks.dat", mgr->data_dir);
    
    FILE *f = fopen(filepath, "wb");
    if (!f) return false;
    
    fwrite(&mgr->count, sizeof(int), 1, f);
    fwrite(mgr->active_list, sizeof(mgr->active_list), 1, f);
    
    for (int i = 0; i < mgr->count; i++) {
        Task *t = &mgr->tasks[i];
        fwrite(t->id, sizeof(t->id), 1, f);
        fwrite(t->text, sizeof(t->text), 1, f);
        fwrite(&t->status, sizeof(TaskStatus), 1, f);
        fwrite(&t->priority, sizeof(int), 1, f);
        fwrite(&t->created, sizeof(FILETIME), 1, f);
        fwrite(&t->completed, sizeof(FILETIME), 1, f);
        fwrite(t->list_name, sizeof(t->list_name), 1, f);
    }
    
    fclose(f);
    return true;
}

bool task_load(TaskManager *mgr) {
    if (mgr->data_dir[0] == '\0') return false;
    
    char filepath[MAX_PATH];
    snprintf(filepath, MAX_PATH, "%s\\tasks.dat", mgr->data_dir);
    
    FILE *f = fopen(filepath, "rb");
    if (!f) return true;
    
    if (fread(&mgr->count, sizeof(int), 1, f) != 1) {
        fclose(f);
        return false;
    }
    
    fread(mgr->active_list, sizeof(mgr->active_list), 1, f);
    
    if (mgr->count > MAX_TASKS) mgr->count = MAX_TASKS;
    
    for (int i = 0; i < mgr->count; i++) {
        Task *t = &mgr->tasks[i];
        fread(t->id, sizeof(t->id), 1, f);
        fread(t->text, sizeof(t->text), 1, f);
        fread(&t->status, sizeof(TaskStatus), 1, f);
        fread(&t->priority, sizeof(int), 1, f);
        fread(&t->created, sizeof(FILETIME), 1, f);
        fread(&t->completed, sizeof(FILETIME), 1, f);
        fread(t->list_name, sizeof(t->list_name), 1, f);
    }
    
    fclose(f);
    return true;
}

bool task_add(TaskManager *mgr, const char *text, const char *list_name, int priority) {
    if (mgr->count >= MAX_TASKS) return false;
    
    Task *t = &mgr->tasks[mgr->count];
    memset(t, 0, sizeof(Task));
    
    SYSTEMTIME st;
    GetSystemTime(&st);
    snprintf(t->id, sizeof(t->id), "task_%04d%02d%02d%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    
    strncpy(t->text, text, MAX_TASK_TEXT - 1);
    strncpy(t->list_name, list_name, MAX_TASK_LIST_NAME - 1);
    t->status = TASK_PENDING;
    t->priority = priority;
    
    GetSystemTimeAsFileTime(&t->created);
    
    mgr->count++;
    task_save(mgr);
    return true;
}

bool task_remove(TaskManager *mgr, int index) {
    if (index < 0 || index >= mgr->count) return false;
    
    for (int i = index; i < mgr->count - 1; i++) {
        mgr->tasks[i] = mgr->tasks[i + 1];
    }
    
    mgr->count--;
    task_save(mgr);
    return true;
}

bool task_update(TaskManager *mgr, int index, const char *text) {
    if (index < 0 || index >= mgr->count) return false;
    
    strncpy(mgr->tasks[index].text, text, MAX_TASK_TEXT - 1);
    task_save(mgr);
    return true;
}

bool task_set_status(TaskManager *mgr, int index, TaskStatus status) {
    if (index < 0 || index >= mgr->count) return false;
    
    mgr->tasks[index].status = status;
    if (status == TASK_DONE) {
        GetSystemTimeAsFileTime(&mgr->tasks[index].completed);
    }
    
    task_save(mgr);
    return true;
}

bool task_set_priority(TaskManager *mgr, int index, int priority) {
    if (index < 0 || index >= mgr->count) return false;
    
    mgr->tasks[index].priority = priority;
    task_save(mgr);
    return true;
}

int task_get_count(TaskManager *mgr) {
    return mgr->count;
}

int task_get_pending_count(TaskManager *mgr) {
    int count = 0;
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->tasks[i].status == TASK_PENDING) count++;
    }
    return count;
}

int task_find_by_id(TaskManager *mgr, const char *id) {
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->tasks[i].id, id) == 0) return i;
    }
    return -1;
}

bool task_move(TaskManager *mgr, int from_index, int to_index) {
    if (from_index < 0 || from_index >= mgr->count) return false;
    if (to_index < 0 || to_index >= mgr->count) return false;
    if (from_index == to_index) return true;
    
    Task tmp = mgr->tasks[from_index];
    
    if (from_index < to_index) {
        for (int i = from_index; i < to_index; i++) {
            mgr->tasks[i] = mgr->tasks[i + 1];
        }
    } else {
        for (int i = from_index; i > to_index; i--) {
            mgr->tasks[i] = mgr->tasks[i - 1];
        }
    }
    
    mgr->tasks[to_index] = tmp;
    task_save(mgr);
    return true;
}

bool task_list_create(TaskManager *mgr, const char *list_name) {
    (void)mgr;
    (void)list_name;
    // Lists are stored in task items, so this is a no-op
    // The first task with this list name creates it
    return true;
}

bool task_list_delete(TaskManager *mgr, const char *list_name) {
    // Remove all tasks in this list
    int i = 0;
    while (i < mgr->count) {
        if (strcmp(mgr->tasks[i].list_name, list_name) == 0) {
            task_remove(mgr, i);
            // Don't increment i, check same index
        } else {
            i++;
        }
    }
    return true;
}

int task_list_get_count(TaskManager *mgr) {
    int lists = 0;
    char seen[MAX_TASKS][MAX_TASK_LIST_NAME];
    memset(seen, 0, sizeof(seen));
    
    for (int i = 0; i < mgr->count; i++) {
        bool found = false;
        for (int j = 0; j < lists; j++) {
            if (strcmp(seen[j], mgr->tasks[i].list_name) == 0) {
                found = true;
                break;
            }
        }
        if (!found && mgr->tasks[i].list_name[0] != '\0') {
            strncpy(seen[lists], mgr->tasks[i].list_name, MAX_TASK_LIST_NAME - 1);
            lists++;
        }
    }
    
    return lists;
}

const char* task_list_get_name(TaskManager *mgr, int index) {
    // Use static storage to avoid returning stack address
    static char result[MAX_TASK_LIST_NAME];
    memset(result, 0, sizeof(result));
    
    int list_count = task_list_get_count(mgr);
    if (index < 0 || index >= list_count) return NULL;
    
    // Collect unique list names
    int found = 0;
    for (int i = 0; i < mgr->count && found <= index; i++) {
        bool is_dup = false;
        for (int j = 0; j < i; j++) {
            if (strcmp(mgr->tasks[i].list_name, mgr->tasks[j].list_name) == 0) {
                is_dup = true;
                break;
            }
        }
        if (!is_dup && mgr->tasks[i].list_name[0] != '\0') {
            if (found == index) {
                strncpy(result, mgr->tasks[i].list_name, MAX_TASK_LIST_NAME - 1);
                return result;
            }
            found++;
        }
    }
    
    return NULL;
}