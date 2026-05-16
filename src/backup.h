/*
 * FastPad Backup System
 * 
 * Provides automatic backup and version history for files.
 */

#ifndef BACKUP_H
#define BACKUP_H

#include "core_types.h"
#include <windows.h>

// Maximum number of backup versions to keep per file
#define MAX_BACKUP_VERSIONS 10

// Maximum backup filename length
#define MAX_BACKUP_FILENAME (MAX_PATH + 64)

// Backup info structure
typedef struct {
    char filepath[MAX_PATH];          // Original file path
    char backup_path[MAX_BACKUP_FILENAME]; // Full backup file path
    SYSTEMTIME timestamp;            // When backup was created
    size_t file_size;                // Size of backed up file
} BackupInfo;

// Backup handle for iteration
typedef struct {
    char original_path[MAX_PATH];    // Original file path
    char filename[MAX_PATH];         // Original filename
    int count;                       // Number of backups found
    int current;                     // Current index during iteration
    BackupInfo info[MAX_BACKUP_VERSIONS]; // Backup info array
} BackupList;

// ============================================================================
// Backup Operations
// ============================================================================

// Initialize backup system
bool backup_init(void);

// Get backup directory path
const char* backup_get_directory(void);

// Create a backup of the current file state
bool backup_create(const char *filepath, const void *data, size_t size);

// List available backups for a file
BackupList* backup_list(const char *filepath);

// Free a backup list
void backup_list_free(BackupList *list);

// Restore a file from backup
bool backup_restore(BackupList *list, int index, void **data, size_t *size);

// Delete old backups (keeps only MAX_BACKUP_VERSIONS most recent)
bool backup_cleanup(const char *filepath);

// Get the number of backups for a file
int backup_count(const char *filepath);

// ============================================================================
// Internal Functions
// ============================================================================

// Generate backup filename from original path and timestamp
bool backup_generate_name(const char *original_path, const SYSTEMTIME *time, 
                         char *backup_path, size_t path_size);

// Ensure backup directory exists
bool backup_ensure_dir(void);

// Parse timestamp from backup filename
bool backup_parse_timestamp(const char *backup_path, SYSTEMTIME *time);

// Compare two SYSTEMTIME values (for sorting)
int backup_time_compare(const SYSTEMTIME *a, const SYSTEMTIME *b);

#endif // BACKUP_H