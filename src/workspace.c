/*
 * FastPad Workspace Management
 * Handles workspace/folder opening and recent workspaces
 */

#include "workspace.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>
#include <sys/stat.h>

// ============================================================================
// Data Directory
// ============================================================================

static char g_data_dir[MAX_PATH] = {0};

const char* workspace_get_data_dir(void) {
    if (g_data_dir[0] == '\0') {
        char appdata[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
            snprintf(g_data_dir, MAX_PATH, "%s\\FastPad\\workspaces", appdata);
            CreateDirectoryA(g_data_dir, NULL);
        }
    }
    return g_data_dir;
}

// ============================================================================
// Path Helpers
// ============================================================================

static bool is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return (st.st_mode & S_IFDIR) != 0;
    }
    return false;
}

static const char* get_basename(const char *path) {
    const char *sep = strrchr(path, '\\');
    if (sep) {
        return sep + 1;
    }
    return path;
}

static void get_display_name(const char *path, char *name, size_t name_size) {
    const char *base = get_basename(path);
    strncpy(name, base, name_size - 1);
    name[name_size - 1] = '\0';
}

// ============================================================================
// Recent Workspaces Storage
// ============================================================================

static const char* get_recent_file_path(void) {
    static char path[MAX_PATH] = {0};
    if (path[0] == '\0') {
        const char *data_dir = workspace_get_data_dir();
        snprintf(path, MAX_PATH, "%s\\recent.json", data_dir);
    }
    return path;
}

void workspace_save_recent(const WorkspaceManager *mgr) {
    if (!mgr) return;
    
    const char *filepath = get_recent_file_path();
    FILE *f = fopen(filepath, "w");
    if (!f) return;
    
    fprintf(f, "{\n  \"recent\": [\n");
    for (int i = 0; i < mgr->recent_count; i++) {
        fprintf(f, "    {\"path\": \"%s\", \"name\": \"%s\"}%s\n",
                mgr->recent[i].path,
                mgr->recent[i].name,
                i < mgr->recent_count - 1 ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
}

void workspace_load_recent(WorkspaceManager *mgr) {
    if (!mgr) return;
    
    mgr->recent_count = 0;
    
    const char *filepath = get_recent_file_path();
    FILE *f = fopen(filepath, "r");
    if (!f) return;
    
    // Simple JSON parsing for recent list
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "\"path\"")) {
            char *start = strchr(line, '"');
            if (start) {
                start += 6; // skip "\"path\": \""
                char *end = strrchr(start, '"');
                if (end && mgr->recent_count < MAX_RECENT_WORKSPACES) {
                    *end = '\0';
                    strncpy(mgr->recent[mgr->recent_count].path, start, MAX_PATH - 1);
                    mgr->recent[mgr->recent_count].path[MAX_PATH - 1] = '\0';
                    
                    // Extract name from path
                    get_display_name(start, mgr->recent[mgr->recent_count].name, MAX_PATH);
                    
                    mgr->recent_count++;
                }
            }
        }
    }
    fclose(f);
}

// ============================================================================
// Workspace Manager
// ============================================================================

void workspace_init(WorkspaceManager *mgr, HWND parent) {
    if (!mgr) return;
    
    memset(mgr, 0, sizeof(WorkspaceManager));
    mgr->parent_hwnd = parent;
    mgr->current.valid = false;
    
    workspace_load_recent(mgr);
}

void workspace_free(WorkspaceManager *mgr) {
    if (!mgr) return;
    workspace_save_recent(mgr);
}

bool workspace_open_folder(WorkspaceManager *mgr) {
    if (!mgr) return false;

    // Use folder browser dialog
    BROWSEINFOA bi = {0};
    bi.hwndOwner = mgr->parent_hwnd;
    bi.lpszTitle = "Select a folder to open as workspace";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (!pidl) return false;
    
    char path[MAX_PATH];
    if (!SHGetPathFromIDListA(pidl, path)) {
        CoTaskMemFree(pidl);
        return false;
    }
    CoTaskMemFree(pidl);
    
    if (!is_directory(path)) {
        return false;
    }
    
    // Set as current workspace
    strncpy(mgr->current.root_path, path, MAX_PATH - 1);
    mgr->current.root_path[MAX_PATH - 1] = '\0';
    mgr->current.valid = true;
    
    // Add to recent
    workspace_add_recent(mgr, path);
    
    log_info("Workspace opened: %s", path);
    return true;
}

void workspace_close(WorkspaceManager *mgr) {
    if (!mgr) return;
    mgr->current.valid = false;
    mgr->current.root_path[0] = '\0';
}

bool workspace_is_open(const WorkspaceManager *mgr) {
    return mgr && mgr->current.valid;
}

const char* workspace_get_root(const WorkspaceManager *mgr) {
    if (!mgr || !mgr->current.valid) return NULL;
    return mgr->current.root_path;
}

void workspace_add_recent(WorkspaceManager *mgr, const char *path) {
    if (!mgr || !path) return;
    
    // Check if already in list
    for (int i = 0; i < mgr->recent_count; i++) {
        if (strcmp(mgr->recent[i].path, path) == 0) {
            // Move to front
            if (i > 0) {
                RecentWorkspace temp = mgr->recent[i];
                memmove(&mgr->recent[1], &mgr->recent[0], i * sizeof(RecentWorkspace));
                mgr->recent[0] = temp;
            }
            workspace_save_recent(mgr);
            return;
        }
    }
    
    // Add to front
    if (mgr->recent_count < MAX_RECENT_WORKSPACES) {
        mgr->recent_count++;
    } else {
        // Shift down
        memmove(&mgr->recent[1], &mgr->recent[0], (MAX_RECENT_WORKSPACES - 1) * sizeof(RecentWorkspace));
    }
    
    strncpy(mgr->recent[0].path, path, MAX_PATH - 1);
    mgr->recent[0].path[MAX_PATH - 1] = '\0';
    get_display_name(path, mgr->recent[0].name, MAX_PATH);
    
    workspace_save_recent(mgr);
}

bool workspace_get_recent(const WorkspaceManager *mgr, int index, char *path, char *name) {
    if (!mgr || index < 0 || index >= mgr->recent_count) return false;
    
    if (path) strncpy(path, mgr->recent[index].path, MAX_PATH - 1);
    if (name) strncpy(name, mgr->recent[index].name, MAX_PATH - 1);
    return true;
}

int workspace_get_recent_count(const WorkspaceManager *mgr) {
    return mgr ? mgr->recent_count : 0;
}

void workspace_clear_recent(WorkspaceManager *mgr) {
    if (!mgr) return;
    mgr->recent_count = 0;
    workspace_save_recent(mgr);
}

// ============================================================================
// File Operations
// ============================================================================

void workspace_reveal_in_explorer(const char *filepath) {
    if (!filepath) return;
    
    // Use explorer /select to reveal file
    char cmd[MAX_PATH + 16];
    snprintf(cmd, sizeof(cmd), "explorer /select,\"%s\"", filepath);
    WinExec(cmd, SW_SHOW);
}

bool workspace_refresh(WorkspaceManager *mgr) {
    (void)mgr;
    // Refresh is handled by explorer module
    return true;
}

bool workspace_new_file(WorkspaceManager *mgr, const char *filename) {
    if (!mgr || !filename) return false;
    if (!workspace_is_open(mgr)) return false;
    
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s\\%s", mgr->current.root_path, filename);
    
    FILE *f = fopen(path, "w");
    if (!f) return false;
    
    fclose(f);
    log_info("Created new file: %s", path);
    return true;
}

bool workspace_rename_file(const char *old_path, const char *new_name) {
    if (!old_path || !new_name) return false;
    
    // Get directory
    char dir[MAX_PATH];
    strncpy(dir, old_path, MAX_PATH - 1);
    dir[MAX_PATH - 1] = '\0';
    
    char *sep = strrchr(dir, '\\');
    if (sep) {
        *sep = '\0';
    } else {
        return false;
    }
    
    // Build new path
    char new_path[MAX_PATH];
    snprintf(new_path, MAX_PATH, "%s\\%s", dir, new_name);
    
    if (rename(old_path, new_path) == 0) {
        log_info("Renamed: %s -> %s", old_path, new_path);
        return true;
    }
    
    return false;
}

bool workspace_delete_file(const char *filepath, HWND parent) {
    if (!filepath) return false;
    
    // Confirm deletion
    const char *name = get_basename(filepath);
    char msg[512];
    snprintf(msg, sizeof(msg), "Are you sure you want to delete \"%s\"?\n\nThis action cannot be undone.", name);
    
    int result = MessageBoxA(parent, msg, "Confirm Delete",
                              MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    
    if (result != IDYES) return false;
    
    if (is_directory(filepath)) {
        // Remove directory
        if (RemoveDirectoryA(filepath)) {
            log_info("Deleted directory: %s", filepath);
            return true;
        }
    } else {
        // Delete file
        if (DeleteFileA(filepath)) {
            log_info("Deleted file: %s", filepath);
            return true;
        }
    }
    
    return false;
}

bool workspace_duplicate_file(const char *filepath, char *new_path, size_t new_path_size) {
    if (!filepath || !new_path) return false;
    
    const char *name = get_basename(filepath);
    const char *ext = strrchr(name, '.');
    
    // Build new name: file.ext -> file_copy.ext
    char base[MAX_PATH];
    if (ext) {
        size_t base_len = ext - name;
        strncpy(base, name, base_len);
        base[base_len] = '\0';
        
        // Check for existing copy
        int copy_num = 1;
        char dir[MAX_PATH];
        strncpy(dir, filepath, MAX_PATH - 1);
        dir[MAX_PATH - 1] = '\0';
        char *sep = strrchr(dir, '\\');
        if (sep) *sep = '\0';
        
        do {
            snprintf(new_path, new_path_size, "%s\\%s_copy%s", dir, base, ext);
            copy_num++;
        } while (GetFileAttributesA(new_path) != INVALID_FILE_ATTRIBUTES && copy_num < 100);
    } else {
        strncpy(base, name, MAX_PATH - 1);
        char dir[MAX_PATH];
        strncpy(dir, filepath, MAX_PATH - 1);
        char *sep = strrchr(dir, '\\');
        if (sep) *sep = '\0';
        
        snprintf(new_path, new_path_size, "%s\\%s_copy", dir, base);
    }
    
    // Copy file
    HANDLE src = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (src == INVALID_HANDLE_VALUE) return false;
    
    HANDLE dst = CreateFileA(new_path, GENERIC_WRITE, 0, NULL,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dst == INVALID_HANDLE_VALUE) {
        CloseHandle(src);
        return false;
    }
    
    // Copy content
    char buffer[8192];
    DWORD bytesRead, bytesWritten;
    bool success = true;
    
    while (ReadFile(src, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        if (!WriteFile(dst, buffer, bytesRead, &bytesWritten, NULL) ||
            bytesWritten != bytesRead) {
            success = false;
            break;
        }
    }
    
    CloseHandle(src);
    CloseHandle(dst);
    
    if (success) {
        log_info("Duplicated: %s -> %s", filepath, new_path);
    }
    
    return success;
}