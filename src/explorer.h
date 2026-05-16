#ifndef EXPLORER_H
#define EXPLORER_H

#include <windows.h>
#include <stdbool.h>

// commctrl.h must come AFTER windows.h as it depends on its types
#include <commctrl.h>

// Command IDs for explorer file operations
#ifndef ID_EXPLORER_FILE_OPEN
#define ID_EXPLORER_FILE_OPEN 4094
#endif

#ifndef ID_GLOBALSEARCH_COMPLETE
#define ID_GLOBALSEARCH_COMPLETE 4095
#endif

// File explorer window control IDs
#define IDC_EXPLORER_TREE 3001
#define IDC_EXPLORER_SEARCH 3002

// Maximum name length
#define MAX_FILE_NAME 256

// File info for tree items
typedef struct {
    char full_path[MAX_PATH];
    char name[MAX_FILE_NAME];
    bool is_directory;
    int children_count;
    int icon_index;
} FileNode;

// Tree item handle wrapper
typedef struct {
    HTREEITEM handle;
    FileNode info;
} TreeItem;

// File icon types
typedef enum {
    ICON_FOLDER,
    ICON_FOLDER_OPEN,
    ICON_FILE_TEXT,
    ICON_FILE_C,
    ICON_FILE_CPP,
    ICON_FILE_H,
    ICON_FILE_PY,
    ICON_FILE_JS,
    ICON_FILE_JSON,
    ICON_FILE_XML,
    ICON_FILE_HTML,
    ICON_FILE_CSS,
    ICON_FILE_MD,
    ICON_FILE_YAML,
    ICON_FILE_SQL,
    ICON_FILE_UNKNOWN,
    ICON_COUNT
} FileIconType;

// File explorer state
typedef struct {
    HWND hwnd;
    HWND parent_hwnd;
    HWND tree_view;
    HFONT font;
    char root_path[MAX_PATH];
    bool visible;
    int width;
    int height;
    HFONT folder_font;
    HFONT file_font;
    HIMAGELIST img_list;
} Explorer;

// Initialize file explorer
bool explorer_init(Explorer *explorer, HWND parent);

// Free file explorer resources
void explorer_free(Explorer *explorer);

// Show/hide explorer
void explorer_show(Explorer *explorer, bool show);

// Is explorer visible
bool explorer_is_visible(const Explorer *explorer);

// Set explorer width
void explorer_set_width(Explorer *explorer, int width);

// Get explorer width
int explorer_get_width(const Explorer *explorer);

// Open folder in explorer
bool explorer_open_folder(Explorer *explorer, const char *path);

// Refresh explorer contents
void explorer_refresh(Explorer *explorer);

// Close workspace (clear explorer)
void explorer_close(Explorer *explorer);

// Handle WM_SIZE
void explorer_on_size(Explorer *explorer, int width, int height);

// Handle WM_COMMAND from explorer controls
bool explorer_on_command(Explorer *explorer, WPARAM wParam, LPARAM lParam);

// Handle tree view notifications
bool explorer_on_notify(Explorer *explorer, LPARAM lParam);

// Get selected file path
bool explorer_get_selected(const Explorer *explorer, char *path, size_t path_size);

// Create new file in explorer
bool explorer_new_file(Explorer *explorer, const char *parent_path);

// Rename item in explorer
bool explorer_rename(Explorer *explorer, HTREEITEM item, const char *new_name);

// Delete item in explorer
bool explorer_delete(Explorer *explorer, HTREEITEM item);

// Duplicate item in explorer
bool explorer_duplicate(Explorer *explorer, HTREEITEM item);

// Reveal item in explorer
void explorer_reveal(Explorer *explorer, HTREEITEM item);

// Get icon for file extension
FileIconType explorer_get_icon_type(const char *filename);

// Build tree item text (name with icon indicator)
void explorer_get_item_text(const FileNode *node, char *buffer, size_t buffer_size);

#endif // EXPLORER_H