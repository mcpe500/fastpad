/*
 * FastPad File Watcher Module Implementation
 * 
 * Monitors files for external changes using Windows ReadDirectoryChangesW.
 */

#include "filewatch.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum number of simultaneous file watches
#define MAX_FILE_WATCHES 32

// Maximum path length
#define MAX_WATCH_PATH 512

// File watch info structure
struct FileWatchInfo {
    char filepath[MAX_WATCH_PATH];
    FileWatchCallback callback;
    void *user_data;
    HANDLE dir_handle;
    HANDLE poll_event;
    OVERLAPPED overlapped;
    FILETIME last_modified;
    bool has_changed;
    bool active;
    char buffer[8192];  // ReadDirectoryChangesW buffer
};

// Global state
static struct {
    FileWatchInfo watches[MAX_FILE_WATCHES];
    int watch_count;
    bool initialized;
} g_filewatch;

// Initialize file watcher system
bool filewatch_init(void) {
    if (g_filewatch.initialized) {
        return true;
    }
    
    memset(&g_filewatch, 0, sizeof(g_filewatch));
    g_filewatch.initialized = true;
    
    log_info("File watcher system initialized");
    return true;
}

// Shutdown file watcher system
void filewatch_shutdown(void) {
    if (!g_filewatch.initialized) {
        return;
    }
    
    filewatch_stop_all();
    g_filewatch.initialized = false;
    
    log_info("File watcher system shut down");
}

// Start watching a file for changes
FileWatchHandle filewatch_start(const char *filepath, FileWatchCallback callback, void *user_data) {
    if (!g_filewatch.initialized) {
        filewatch_init();
    }
    
    if (!filepath || !callback) {
        return NULL;
    }
    
    if (g_filewatch.watch_count >= MAX_FILE_WATCHES) {
        log_error("Maximum file watch count reached");
        return NULL;
    }
    
    // Find free slot
    FileWatchInfo *watch = NULL;
    for (int i = 0; i < MAX_FILE_WATCHES; i++) {
        if (!g_filewatch.watches[i].active) {
            watch = &g_filewatch.watches[i];
            break;
        }
    }
    
    if (!watch) {
        return NULL;
    }
    
    memset(watch, 0, sizeof(FileWatchInfo));
    
    // Copy filepath
    strncpy(watch->filepath, filepath, MAX_WATCH_PATH - 1);
    watch->filepath[MAX_WATCH_PATH - 1] = '\0';
    
    // Store callback and user data
    watch->callback = callback;
    watch->user_data = user_data;
    watch->active = true;
    watch->has_changed = false;
    
    // Get directory containing the file
    char dir_path[MAX_WATCH_PATH];
    strncpy(dir_path, filepath, MAX_WATCH_PATH - 1);
    dir_path[MAX_WATCH_PATH - 1] = '\0';
    
    // Find last backslash
    char *last_bs = strrchr(dir_path, '\\');
    if (last_bs) {
        *last_bs = '\0';
    } else {
        // No directory component, use current directory
        dir_path[0] = '.';
        dir_path[1] = '\0';
    }
    
    // Open directory handle for monitoring
    watch->dir_handle = CreateFileA(
        dir_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );
    
    if (watch->dir_handle == INVALID_HANDLE_VALUE) {
        log_error("Failed to open directory for file watching: %s", dir_path);
        watch->active = false;
        return NULL;
    }
    
    // Get current file modification time
    HANDLE file_h = CreateFileA(filepath, GENERIC_READ, 
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_EXISTING, 0, NULL);
    if (file_h != INVALID_HANDLE_VALUE) {
        GetFileTime(file_h, NULL, NULL, &watch->last_modified);
        CloseHandle(file_h);
    }
    
    // Create event for overlapped I/O
    watch->poll_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    memset(&watch->overlapped, 0, sizeof(watch->overlapped));
    watch->overlapped.hEvent = watch->poll_event;
    
    g_filewatch.watch_count++;
    
    log_info("Started watching file: %s", filepath);
    
    return (FileWatchHandle)watch;
}

// Stop watching a file
bool filewatch_stop(FileWatchHandle handle) {
    if (!handle) {
        return false;
    }
    
    FileWatchInfo *watch = (FileWatchInfo*)handle;
    
    if (!watch->active) {
        return false;
    }
    
    // Close handles
    if (watch->dir_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(watch->dir_handle);
        watch->dir_handle = INVALID_HANDLE_VALUE;
    }
    
    if (watch->poll_event) {
        CloseHandle(watch->poll_event);
        watch->poll_event = NULL;
    }
    
    watch->active = false;
    g_filewatch.watch_count--;
    
    log_info("Stopped watching file: %s", watch->filepath);
    
    return true;
}

// Stop all watchers
void filewatch_stop_all(void) {
    for (int i = 0; i < MAX_FILE_WATCHES; i++) {
        if (g_filewatch.watches[i].active) {
            filewatch_stop((FileWatchHandle)&g_filewatch.watches[i]);
        }
    }
}

// Check if a specific file has changed
bool filewatch_has_changed(const char *filepath) {
    if (!filepath) {
        return false;
    }
    
    for (int i = 0; i < MAX_FILE_WATCHES; i++) {
        FileWatchInfo *watch = &g_filewatch.watches[i];
        if (watch->active && strcmp(watch->filepath, filepath) == 0) {
            return watch->has_changed;
        }
    }
    
    return false;
}

// Clear the changed flag for a file
void filewatch_clear_changed(const char *filepath) {
    if (!filepath) {
        return;
    }
    
    for (int i = 0; i < MAX_FILE_WATCHES; i++) {
        FileWatchInfo *watch = &g_filewatch.watches[i];
        if (watch->active && strcmp(watch->filepath, filepath) == 0) {
            watch->has_changed = false;
            
            // Update last modified time
            HANDLE file_h = CreateFileA(filepath, GENERIC_READ, 
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL, OPEN_EXISTING, 0, NULL);
            if (file_h != INVALID_HANDLE_VALUE) {
                GetFileTime(file_h, NULL, NULL, &watch->last_modified);
                CloseHandle(file_h);
            }
            return;
        }
    }
}

// Get the last modification time for a watched file
bool filewatch_get_modified_time(const char *filepath, FILETIME *out_time) {
    if (!filepath || !out_time) {
        return false;
    }
    
    HANDLE file_h = CreateFileA(filepath, GENERIC_READ, 
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_EXISTING, 0, NULL);
    if (file_h == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    BOOL result = GetFileTime(file_h, NULL, NULL, out_time);
    CloseHandle(file_h);
    
    return result != 0;
}

// Process a single watch handle - check for file changes
void filewatch_process_handle(FileWatchHandle handle) {
    if (!handle) {
        return;
    }
    
    FileWatchInfo *watch = (FileWatchInfo*)handle;
    
    if (!watch->active) {
        return;
    }
    
    // Check if file modification time has changed
    FILETIME new_time;
    if (filewatch_get_modified_time(watch->filepath, &new_time)) {
        // Compare with last known time
        if (CompareFileTime(&new_time, &watch->last_modified) > 0) {
            // File has changed
            if (!watch->has_changed) {
                watch->has_changed = true;
                
                log_info("File changed externally: %s", watch->filepath);
                
                // Call callback if set
                if (watch->callback) {
                    watch->callback(watch->filepath, watch->user_data);
                }
            }
        }
    }
}

// Check for file changes - should be called periodically
void filewatch_poll(void) {
    if (!g_filewatch.initialized) {
        return;
    }
    
    for (int i = 0; i < MAX_FILE_WATCHES; i++) {
        if (g_filewatch.watches[i].active) {
            filewatch_process_handle((FileWatchHandle)&g_filewatch.watches[i]);
        }
    }
}

// Check if file content has changed by comparing with current buffer content
bool filewatch_check_content_change(const char *filepath, const char *current_content, int current_len) {
    if (!filepath || !current_content) {
        return false;
    }
    
    // Read file from disk
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    int file_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_len != current_len) {
        fclose(f);
        return true;  // Length changed
    }
    
    char *file_content = (char*)malloc(file_len + 1);
    if (!file_content) {
        fclose(f);
        return false;
    }
    
    size_t read = fread(file_content, 1, file_len, f);
    fclose(f);
    
    if ((int)read != file_len) {
        free(file_content);
        return true;
    }
    
    file_content[file_len] = '\0';
    
    // Compare
    bool changed = (memcmp(file_content, current_content, current_len) != 0);
    
    free(file_content);
    return changed;
}