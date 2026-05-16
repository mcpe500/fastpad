/*
 * FastPad Backup System Implementation
 * 
 * Provides automatic backup and version history for files.
 */

#include "backup.h"
#include "log.h"
#include "core_types.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

// Global backup directory
static char g_backup_dir[MAX_PATH] = {0};

// ============================================================================
// Initialization
// ============================================================================

bool backup_init(void) {
    char appdata[MAX_PATH];
    
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) != S_OK) {
        log_error("Failed to get AppData folder path");
        return false;
    }
    
    snprintf(g_backup_dir, MAX_PATH, "%s\\FastPad\\backups", appdata);
    
    // Ensure directory exists
    if (!backup_ensure_dir()) {
        log_error("Failed to create backup directory: %s", g_backup_dir);
        return false;
    }
    
    log_info("Backup system initialized: %s", g_backup_dir);
    return true;
}

const char* backup_get_directory(void) {
    return g_backup_dir;
}

bool backup_ensure_dir(void) {
    if (g_backup_dir[0] == '\0') {
        return false;
    }
    
    // Create directory tree if it doesn't exist
    char dir[MAX_PATH];
    strncpy(dir, g_backup_dir, MAX_PATH - 1);
    dir[MAX_PATH - 1] = '\0';
    
    // Create each level of the directory path
    for (char *p = dir + 3; *p; p++) {
        if (*p == '\\') {
            *p = '\0';
            CreateDirectoryA(dir, NULL);
            *p = '\\';
        }
    }
    
    return true;
}

// ============================================================================
// Backup Creation
// ============================================================================

bool backup_create(const char *filepath, const void *data, size_t size) {
    if (!filepath || !data || size == 0) {
        return false;
    }
    
    // Ensure backup directory exists
    if (!backup_ensure_dir()) {
        return false;
    }
    
    // Get current timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    // Generate backup filename
    char backup_path[MAX_BACKUP_FILENAME];
    if (!backup_generate_name(filepath, &st, backup_path, sizeof(backup_path))) {
        log_error("Failed to generate backup filename for: %s", filepath);
        return false;
    }
    
    // Write backup file
    FILE *f = fopen(backup_path, "wb");
    if (!f) {
        log_error("Failed to create backup file: %s", backup_path);
        return false;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written != size) {
        log_error("Failed to write complete backup data");
        DeleteFileA(backup_path);
        return false;
    }
    
    log_info("Created backup: %s", backup_path);
    
    // Cleanup old backups
    backup_cleanup(filepath);
    
    return true;
}

bool backup_generate_name(const char *original_path, const SYSTEMTIME *time,
                         char *backup_path, size_t path_size) {
    if (!original_path || !time || !backup_path) {
        return false;
    }
    
    // Extract just the filename from the full path
    const char *filename = strrchr(original_path, '\\');
    if (filename) {
        filename++;
    } else {
        filename = original_path;
    }
    
    // Create backup filename: "originalname_timestamp.bak"
    // Timestamp format: YYYY-MM-DD_HH-MM-SS
    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d_%02d-%02d-%02d",
             time->wYear, time->wMonth, time->wDay,
             time->wHour, time->wMinute, time->wSecond);
    
    // Get just the filename without extension for backup naming
    char name_only[MAX_PATH];
    strncpy(name_only, filename, MAX_PATH - 1);
    name_only[MAX_PATH - 1] = '\0';
    
    // Remove extension if present
    char *ext = strrchr(name_only, '.');
    if (ext) {
        *ext = '\0';
    }
    
    // Build full backup path
    snprintf(backup_path, path_size, "%s\\%s_%s.bak",
             g_backup_dir, name_only, timestamp);
    
    return true;
}

// ============================================================================
// Backup Listing
// ============================================================================

BackupList* backup_list(const char *filepath) {
    if (!filepath) {
        return NULL;
    }
    
    BackupList *list = (BackupList *)malloc(sizeof(BackupList));
    if (!list) {
        return NULL;
    }
    memset(list, 0, sizeof(BackupList));
    
    // Extract just the filename
    const char *filename = strrchr(filepath, '\\');
    if (filename) {
        filename++;
    } else {
        filename = filepath;
    }
    
    strncpy(list->original_path, filepath, MAX_PATH - 1);
    strncpy(list->filename, filename, MAX_PATH - 1);
    list->count = 0;
    list->current = 0;
    
    // Get original name without extension
    strncpy(list->original_path, filename, MAX_PATH - 1);
    char *ext = strrchr(list->original_path, '.');
    if (ext) {
        *ext = '\0';
    }
    
    // Search for matching backup files
    // Pattern: originalname_*.bak
    char search_pattern[MAX_PATH];
    snprintf(search_pattern, MAX_PATH, "%s\\%s_*.bak", g_backup_dir, list->original_path);
    
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return list;  // No backups found, return empty list
    }
    
    do {
        if (list->count >= MAX_BACKUP_VERSIONS) {
            break;  // Reached max versions
        }
        
        // Build full backup path
        snprintf(list->info[list->count].backup_path, MAX_BACKUP_FILENAME,
                 "%s\\%s", g_backup_dir, fd.cFileName);
        
        strncpy(list->info[list->count].filepath, filepath, MAX_PATH - 1);
        
        // Parse timestamp from filename
        if (backup_parse_timestamp(fd.cFileName, &list->info[list->count].timestamp)) {
            list->info[list->count].file_size = fd.nFileSizeLow;
            list->count++;
        }
        
    } while (FindNextFileA(hFind, &fd));
    
    FindClose(hFind);
    
    // Sort backups by timestamp (newest first)
    for (int i = 0; i < list->count - 1; i++) {
        for (int j = i + 1; j < list->count; j++) {
            if (backup_time_compare(&list->info[i].timestamp, &list->info[j].timestamp) < 0) {
                BackupInfo temp = list->info[i];
                list->info[i] = list->info[j];
                list->info[j] = temp;
            }
        }
    }
    
    return list;
}

void backup_list_free(BackupList *list) {
    if (list) {
        free(list);
    }
}

bool backup_parse_timestamp(const char *backup_path, SYSTEMTIME *time) {
    if (!backup_path || !time) {
        return false;
    }
    
    // Expected format: name_YYYY-MM-DD_HH-MM-SS.bak
    // Find the timestamp part after the last underscore before .bak
    
    const char *underscore = strrchr(backup_path, '_');
    if (!underscore) {
        return false;
    }
    
    // Find .bak extension
    const char *ext = strstr(backup_path, ".bak");
    if (!ext) {
        return false;
    }
    
    // Extract timestamp string
    char timestamp[64];
    int len = (int)(ext - underscore - 1);  // -1 to exclude underscore
    if (len <= 0 || len >= (int)sizeof(timestamp)) {
        return false;
    }
    
    strncpy(timestamp, underscore + 1, len);
    timestamp[len] = '\0';
    
    // Parse timestamp: YYYY-MM-DD_HH-MM-SS
    int year, month, day, hour, minute, second;
    int parsed = sscanf(timestamp, "%d-%d-%d_%d-%d-%d",
                        &year, &month, &day, &hour, &minute, &second);
    
    if (parsed != 6) {
        return false;
    }
    
    memset(time, 0, sizeof(SYSTEMTIME));
    time->wYear = (WORD)year;
    time->wMonth = (WORD)month;
    time->wDay = (WORD)day;
    time->wHour = (WORD)hour;
    time->wMinute = (WORD)minute;
    time->wSecond = (WORD)second;
    time->wMilliseconds = 0;
    
    return true;
}

int backup_time_compare(const SYSTEMTIME *a, const SYSTEMTIME *b) {
    if (!a || !b) {
        return 0;
    }
    
    if (a->wYear != b->wYear) return a->wYear - b->wYear;
    if (a->wMonth != b->wMonth) return a->wMonth - b->wMonth;
    if (a->wDay != b->wDay) return a->wDay - b->wDay;
    if (a->wHour != b->wHour) return a->wHour - b->wHour;
    if (a->wMinute != b->wMinute) return a->wMinute - b->wMinute;
    if (a->wSecond != b->wSecond) return a->wSecond - b->wSecond;
    
    return 0;
}

// ============================================================================
// Backup Restoration
// ============================================================================

bool backup_restore(BackupList *list, int index, void **data, size_t *size) {
    if (!list || index < 0 || index >= list->count || !data || !size) {
        return false;
    }
    
    const char *backup_path = list->info[index].backup_path;
    
    // Open backup file
    FILE *f = fopen(backup_path, "rb");
    if (!f) {
        log_error("Failed to open backup file: %s", backup_path);
        return false;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(f);
        return false;
    }
    
    // Allocate buffer
    char *buffer = (char *)malloc((size_t)file_size);
    if (!buffer) {
        fclose(f);
        log_error("Out of memory reading backup");
        return false;
    }
    
    // Read file content
    size_t read = fread(buffer, 1, (size_t)file_size, f);
    fclose(f);
    
    if (read != (size_t)file_size) {
        free(buffer);
        return false;
    }
    
    *data = buffer;
    *size = (size_t)file_size;
    
    log_info("Restored backup: %s", backup_path);
    return true;
}

int backup_count(const char *filepath) {
    BackupList *list = backup_list(filepath);
    if (!list) {
        return 0;
    }
    int count = list->count;
    backup_list_free(list);
    return count;
}

// ============================================================================
// Backup Cleanup
// ============================================================================

bool backup_cleanup(const char *filepath) {
    if (!filepath) {
        return false;
    }
    
    BackupList *list = backup_list(filepath);
    if (!list || list->count <= MAX_BACKUP_VERSIONS) {
        if (list) backup_list_free(list);
        return true;  // Nothing to clean up
    }
    
    // Delete oldest backups beyond MAX_BACKUP_VERSIONS
    for (int i = MAX_BACKUP_VERSIONS; i < list->count; i++) {
        if (DeleteFileA(list->info[i].backup_path)) {
            log_info("Deleted old backup: %s", list->info[i].backup_path);
        } else {
            log_error("Failed to delete backup: %s", list->info[i].backup_path);
        }
    }
    
    backup_list_free(list);
    return true;
}