#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <windows.h>
#include <stdbool.h>

// Maximum recent workspaces
#define MAX_RECENT_WORKSPACES 10
#define MAX_PATH_LENGTH MAX_PATH

// Workspace state
typedef struct {
    char root_path[MAX_PATH];
    bool valid;
} Workspace;

// Recent workspace entry
typedef struct {
    char path[MAX_PATH];
    char name[MAX_PATH];
} RecentWorkspace;

// Workspace manager
typedef struct {
    Workspace current;
    RecentWorkspace recent[MAX_RECENT_WORKSPACES];
    int recent_count;
    HWND parent_hwnd;
} WorkspaceManager;

// Get the workspaces data directory
const char* workspace_get_data_dir(void);

// Initialize workspace manager
void workspace_init(WorkspaceManager *mgr, HWND parent);

// Free workspace manager resources
void workspace_free(WorkspaceManager *mgr);

// Open folder dialog and set as workspace
bool workspace_open_folder(WorkspaceManager *mgr);

// Close current workspace
void workspace_close(WorkspaceManager *mgr);

// Check if workspace is open
bool workspace_is_open(const WorkspaceManager *mgr);

// Get current workspace root path
const char* workspace_get_root(const WorkspaceManager *mgr);

// Add path to recent workspaces
void workspace_add_recent(WorkspaceManager *mgr, const char *path);

// Get recent workspace by index
bool workspace_get_recent(const WorkspaceManager *mgr, int index, char *path, char *name);

// Get recent workspace count
int workspace_get_recent_count(const WorkspaceManager *mgr);

// Clear recent workspaces
void workspace_clear_recent(WorkspaceManager *mgr);

// Save recent workspaces to disk
void workspace_save_recent(const WorkspaceManager *mgr);

// Load recent workspaces from disk
void workspace_load_recent(WorkspaceManager *mgr);

// Reveal file in explorer
void workspace_reveal_in_explorer(const char *filepath);

// Refresh workspace (rescan directory)
bool workspace_refresh(WorkspaceManager *mgr);

// Create new file in workspace
bool workspace_new_file(WorkspaceManager *mgr, const char *filename);

// Rename file in workspace
bool workspace_rename_file(const char *old_path, const char *new_name);

// Delete file with confirmation
bool workspace_delete_file(const char *filepath, HWND parent);

// Duplicate file
bool workspace_duplicate_file(const char *filepath, char *new_path, size_t new_path_size);

#endif // WORKSPACE_H
