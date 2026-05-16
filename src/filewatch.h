/*
 * FastPad File Watcher Module
 * 
 * Monitors files for external changes and notifies the application.
 */

#ifndef FILEWATCH_H
#define FILEWATCH_H

#include "core_types.h"
#include <windows.h>

// File watcher handle
typedef void* FileWatchHandle;

// File watch callback function type
typedef void (*FileWatchCallback)(const char *filepath, void *user_data);

// ============================================================================
// File Watcher Functions
// ============================================================================

// Initialize file watcher system
bool filewatch_init(void);

// Shutdown file watcher system
void filewatch_shutdown(void);

// Start watching a file for changes
// Returns handle for later use, or NULL on failure
FileWatchHandle filewatch_start(const char *filepath, FileWatchCallback callback, void *user_data);

// Stop watching a file
bool filewatch_stop(FileWatchHandle handle);

// Stop all watchers
void filewatch_stop_all(void);

// Check if a file has changed externally and trigger callback
// Should be called periodically from the main loop
void filewatch_poll(void);

// Check if a specific file has pending changes
bool filewatch_has_changed(const char *filepath);

// Clear the changed flag for a file
void filewatch_clear_changed(const char *filepath);

// Get the last modification time for a watched file
bool filewatch_get_modified_time(const char *filepath, FILETIME *out_time);

// ============================================================================
// Internal/Helper Functions
// ============================================================================

// Process a single watch handle
void filewatch_process_handle(FileWatchHandle handle);

// Determine if file content has changed by re-reading and comparing
bool filewatch_check_content_change(const char *filepath, const char *current_content, int current_len);

// Watch info structure (opaque)
typedef struct FileWatchInfo FileWatchInfo;

#endif // FILEWATCH_H