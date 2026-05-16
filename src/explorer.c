/*
 * FastPad File Explorer
 * Sidebar file tree view
 */

#include "explorer.h"
#include "workspace.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

// ============================================================================
// Icon Helpers
// ============================================================================

FileIconType explorer_get_icon_type(const char *filename) {
    if (!filename) return ICON_FILE_UNKNOWN;
    
    const char *ext = strrchr(filename, '.');
    if (!ext) {
        // No extension - could be a makefile or other special file
        if (_stricmp(filename, "Makefile") == 0 || _stricmp(filename, "makefile") == 0) {
            return ICON_FILE_C;
        }
        return ICON_FILE_TEXT;
    }
    ext++; // Skip the dot
    
    // Text files
    if (_stricmp(ext, "txt") == 0) return ICON_FILE_TEXT;
    if (_stricmp(ext, "md") == 0) return ICON_FILE_MD;
    if (_stricmp(ext, "markdown") == 0) return ICON_FILE_MD;
    
    // Source files
    if (_stricmp(ext, "c") == 0) return ICON_FILE_C;
    if (_stricmp(ext, "h") == 0) return ICON_FILE_H;
    if (_stricmp(ext, "cpp") == 0) return ICON_FILE_CPP;
    if (_stricmp(ext, "cxx") == 0) return ICON_FILE_CPP;
    if (_stricmp(ext, "cc") == 0) return ICON_FILE_CPP;
    if (_stricmp(ext, "hpp") == 0) return ICON_FILE_H;
    if (_stricmp(ext, "hxx") == 0) return ICON_FILE_H;
    if (_stricmp(ext, "py") == 0) return ICON_FILE_PY;
    if (_stricmp(ext, "js") == 0) return ICON_FILE_JS;
    if (_stricmp(ext, "ts") == 0) return ICON_FILE_JS;
    if (_stricmp(ext, "jsx") == 0) return ICON_FILE_JS;
    if (_stricmp(ext, "tsx") == 0) return ICON_FILE_JS;
    
    // Data files
    if (_stricmp(ext, "json") == 0) return ICON_FILE_JSON;
    if (_stricmp(ext, "xml") == 0) return ICON_FILE_XML;
    if (_stricmp(ext, "html") == 0) return ICON_FILE_HTML;
    if (_stricmp(ext, "htm") == 0) return ICON_FILE_HTML;
    if (_stricmp(ext, "css") == 0) return ICON_FILE_CSS;
    if (_stricmp(ext, "scss") == 0) return ICON_FILE_CSS;
    if (_stricmp(ext, "sass") == 0) return ICON_FILE_CSS;
    if (_stricmp(ext, "yaml") == 0) return ICON_FILE_YAML;
    if (_stricmp(ext, "yml") == 0) return ICON_FILE_YAML;
    if (_stricmp(ext, "sql") == 0) return ICON_FILE_SQL;
    
    return ICON_FILE_UNKNOWN;
}

void explorer_get_item_text(const FileNode *node, char *buffer, size_t buffer_size) {
    if (!node || !buffer) return;
    
    // Format: icon + name
    const char *icon = node->is_directory ? "[+]" : "  ";
    snprintf(buffer, buffer_size, "%s %s", icon, node->name);
}

// ============================================================================
// Tree Building
// ============================================================================

static HTREEITEM add_tree_item(Explorer *explorer, HTREEITEM parent, const FileNode *node) {
    TVINSERTSTRUCTA tvi = {0};
    tvi.hParent = parent;
    tvi.hInsertAfter = TVI_SORT;
    tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
    
    // Create display text
    char text[MAX_FILE_NAME + 5];
    explorer_get_item_text(node, text, sizeof(text));
    
    tvi.item.pszText = text;
    tvi.item.lParam = (LPARAM)strdup(node->full_path);
    
    return TreeView_InsertItem(explorer->tree_view, &tvi);
}

static bool scan_directory(Explorer *explorer, HTREEITEM parent, const char *path) {
    if (!explorer || !path) return false;
    
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*", path);
    
    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(search_path, &find_data);
    
    if (find == INVALID_HANDLE_VALUE) return false;
    
    do {
        const char *name = find_data.cFileName;
        
        // Skip . and ..
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        
        // Build full path
        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s\\%s", path, name);
        
        FileNode node = {0};
        strncpy(node.name, name, MAX_FILE_NAME - 1);
        strncpy(node.full_path, full_path, MAX_PATH - 1);
        node.is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        
        HTREEITEM item = add_tree_item(explorer, parent, &node);
        
        // If directory, add placeholder for expansion
        if (node.is_directory) {
            // Add a dummy child so tree shows +/- correctly
            TVINSERTSTRUCTA tvic = {0};
            tvic.hParent = item;
            tvic.hInsertAfter = TVI_LAST;
            tvic.item.mask = TVIF_TEXT;
            tvic.item.pszText = "";
            TreeView_InsertItem(explorer->tree_view, &tvic);
        }
        
    } while (FindNextFileA(find, &find_data));
    
    FindClose(find);
    return true;
}

// ============================================================================
// Context Menu
// ============================================================================

static void show_context_menu(Explorer *explorer, int x, int y, HTREEITEM item) {
    HMENU menu = CreatePopupMenu();
    
    // Get selected node info
    TVITEM ti = {0};
    ti.hItem = item;
    ti.mask = TVIF_PARAM;
    char path[MAX_PATH] = {0};
    
    if (TreeView_GetItem(explorer->tree_view, &ti)) {
        strncpy(path, (const char*)ti.lParam, MAX_PATH - 1);
    }
    
    // Check if it's a directory - used for context menu
    bool is_dir = GetFileAttributesA(path) & FILE_ATTRIBUTE_DIRECTORY;
    (void)is_dir; // Suppress unused warning
    
    AppendMenuA(menu, MF_STRING, 1, "New File");
    AppendMenuA(menu, MF_STRING, 2, "Rename");
    AppendMenuA(menu, MF_STRING, 3, "Delete");
    AppendMenuA(menu, MF_STRING, 4, "Duplicate");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, 5, "Reveal in Explorer");
    AppendMenuA(menu, MF_STRING, 6, "Refresh");
    
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                              x, y, 0, explorer->hwnd, NULL);
    
    switch (cmd) {
        case 1: // New File
            explorer_new_file(explorer, path);
            break;
        case 2: // Rename
            explorer_rename(explorer, item, NULL);
            break;
        case 3: // Delete
            explorer_delete(explorer, item);
            break;
        case 4: // Duplicate
            explorer_duplicate(explorer, item);
            break;
        case 5: // Reveal
            explorer_reveal(explorer, item);
            break;
        case 6: // Refresh
            explorer_refresh(explorer);
            break;
    }
    
    DestroyMenu(menu);
}

// ============================================================================
// Explorer Implementation
// ============================================================================

bool explorer_init(Explorer *explorer, HWND parent) {
    if (!explorer) return false;
    
    memset(explorer, 0, sizeof(Explorer));
    explorer->parent_hwnd = parent;
    explorer->visible = true;
    explorer->width = 250;
    
    // Create tree view window
    explorer->tree_view = CreateWindowExA(
        0,
        WC_TREEVIEWA,
        NULL,
        WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT,
        0, 0,
        explorer->width, 0,
        parent,
        (HMENU)IDC_EXPLORER_TREE,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!explorer->tree_view) return false;
    
    explorer->hwnd = explorer->tree_view;
    
    // Set font
    HFONT hFont = (HFONT)SendMessage(parent, WM_GETFONT, 0, 0);
    if (hFont) {
        SendMessage(explorer->tree_view, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    return true;
}

void explorer_free(Explorer *explorer) {
    if (!explorer) return;
    
    if (explorer->tree_view) {
        DestroyWindow(explorer->tree_view);
        explorer->tree_view = NULL;
    }
}

void explorer_show(Explorer *explorer, bool show) {
    if (!explorer) return;
    explorer->visible = show;
    ShowWindow(explorer->tree_view, show ? SW_SHOW : SW_HIDE);
}

bool explorer_is_visible(const Explorer *explorer) {
    return explorer && explorer->visible;
}

void explorer_set_width(Explorer *explorer, int width) {
    if (explorer) explorer->width = width;
}

int explorer_get_width(const Explorer *explorer) {
    return explorer ? explorer->width : 0;
}

bool explorer_open_folder(Explorer *explorer, const char *path) {
    if (!explorer || !path) return false;
    
    // Store root path
    strncpy(explorer->root_path, path, MAX_PATH - 1);
    explorer->root_path[MAX_PATH - 1] = '\0';
    
    // Clear existing items
    TreeView_DeleteAllItems(explorer->tree_view);
    
    // Build tree from root
    FileNode root_node = {0};
    strncpy(root_node.name, path, MAX_FILE_NAME - 1);
    
    // Get just folder name
    const char *folder = strrchr(path, '\\');
    if (folder) folder++;
    else folder = path;
    strncpy(root_node.name, folder, MAX_FILE_NAME - 1);
    strncpy(root_node.full_path, path, MAX_PATH - 1);
    root_node.is_directory = true;
    
    HTREEITEM root = add_tree_item(explorer, TVI_ROOT, &root_node);
    
    // Scan and populate
    scan_directory(explorer, root, path);
    
    // Expand root
    TreeView_Expand(explorer->tree_view, root, TVE_EXPAND);
    
    log_info("Explorer opened folder: %s", path);
    return true;
}

void explorer_refresh(Explorer *explorer) {
    if (!explorer) return;
    
    if (explorer->root_path[0]) {
        explorer_open_folder(explorer, explorer->root_path);
    }
}

void explorer_close(Explorer *explorer) {
    if (!explorer) return;
    explorer->root_path[0] = '\0';
    TreeView_DeleteAllItems(explorer->tree_view);
}

void explorer_on_size(Explorer *explorer, int width, int height) {
    if (!explorer) return;
    SetWindowPos(explorer->tree_view, NULL, 0, 0, width, height, SWP_NOZORDER);
}

bool explorer_on_command(Explorer *explorer, WPARAM wParam, LPARAM lParam) {
    (void)explorer;
    (void)wParam;
    (void)lParam;
    return false;
}

bool explorer_on_notify(Explorer *explorer, LPARAM lParam) {
    if (!explorer) return false;
    
    LPNMTREEVIEWA nmtv = (LPNMTREEVIEWA)lParam;
    
    switch (nmtv->hdr.code) {
        case TVN_ITEMEXPANDINGA: {
            HTREEITEM item = nmtv->itemNew.hItem;
            if (!item) break;
            
            // Get full path from lParam
            TVITEM tvi = {0};
            tvi.hItem = item;
            tvi.mask = TVIF_PARAM;
            if (!TreeView_GetItem(explorer->tree_view, &tvi)) break;
            
            const char *path = (const char*)tvi.lParam;
            if (!path) break;
            
            // Check if directory
            if (GetFileAttributesA(path) & FILE_ATTRIBUTE_DIRECTORY) {
                // Delete existing children
                HTREEITEM child = TreeView_GetChild(explorer->tree_view, item);
                while (child) {
                    HTREEITEM next = TreeView_GetNextSibling(explorer->tree_view, child);
                    SendMessage(explorer->tree_view, TVM_DELETEITEM, 0, (LPARAM)child);
                    child = next;
                }
                
                // Rescan directory
                scan_directory(explorer, item, path);
            }
            return TRUE;
        }
        
        case NM_DBLCLK: {
            // Get clicked item
            TVHITTESTINFO ht = {0};
            DWORD pos = GetMessagePos();
            ht.pt.x = LOWORD(pos);
            ht.pt.y = HIWORD(pos);
            ScreenToClient(explorer->tree_view, &ht.pt);
            
            HTREEITEM item = TreeView_HitTest(explorer->tree_view, &ht);
            if (item && !(ht.flags & TVHT_ONITEMBUTTON)) {
                // Double-click on item - if file, open it
                TVITEM tvi = {0};
                tvi.hItem = item;
                tvi.mask = TVIF_PARAM;
                if (TreeView_GetItem(explorer->tree_view, &tvi)) {
                    const char *path = (const char*)tvi.lParam;
                    if (path && !(GetFileAttributesA(path) & FILE_ATTRIBUTE_DIRECTORY)) {
                        // File - notify parent to open
                        SendMessage(explorer->parent_hwnd, WM_COMMAND, 
                                   MAKEWPARAM(ID_EXPLORER_FILE_OPEN, 0), (LPARAM)strdup(path));
                    }
                }
            }
            return TRUE;
        }
        
        case NM_RCLICK: {
            // Right-click context menu
            TVHITTESTINFO ht = {0};
            DWORD pos = GetMessagePos();
            ht.pt.x = LOWORD(pos);
            ht.pt.y = HIWORD(pos);
            ScreenToClient(explorer->tree_view, &ht.pt);
            
            HTREEITEM item = TreeView_HitTest(explorer->tree_view, &ht);
            if (item && (ht.flags & TVHT_ONITEM)) {
                TreeView_SelectItem(explorer->tree_view, item);
                show_context_menu(explorer, ht.pt.x, ht.pt.y, item);
            }
            return TRUE;
        }
        
        case TVN_BEGINDRAGA: {
            // Start drag operation - not implemented yet
            return TRUE;
        }
    }
    
    return FALSE;
}

bool explorer_get_selected(const Explorer *explorer, char *path, size_t path_size) {
    if (!explorer || !path) return false;
    
    HTREEITEM item = TreeView_GetSelection(explorer->tree_view);
    if (!item) return false;
    
    TVITEM tvi = {0};
    tvi.hItem = item;
    tvi.mask = TVIF_PARAM;
    
    if (TreeView_GetItem(explorer->tree_view, &tvi)) {
        strncpy(path, (const char*)tvi.lParam, path_size - 1);
        path[path_size - 1] = '\0';
        return true;
    }
    
    return false;
}

bool explorer_new_file(Explorer *explorer, const char *parent_path) {
    if (!explorer || !parent_path) return false;
    
    // Simple input dialog for filename
    const char *default_name = "newfile.txt";
    
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s\\%s", parent_path, default_name);
    
    // Check if exists
    if (GetFileAttributesA(full_path) != INVALID_FILE_ATTRIBUTES) {
        // Already exists, generate unique name
        for (int i = 1; i < 100; i++) {
            char try_name[MAX_FILE_NAME];
            snprintf(try_name, MAX_FILE_NAME, "newfile%d.txt", i);
            snprintf(full_path, MAX_PATH, "%s\\%s", parent_path, try_name);
            if (GetFileAttributesA(full_path) == INVALID_FILE_ATTRIBUTES) {
                break;
            }
        }
    }
    
    FILE *f = fopen(full_path, "w");
    if (f) {
        fclose(f);
        explorer_refresh(explorer);
        return true;
    }
    
    return false;
}

bool explorer_rename(Explorer *explorer, HTREEITEM item, const char *new_name) {
    if (!explorer || !item) return false;
    
    // Get current path
    TVITEM tvi = {0};
    tvi.hItem = item;
    tvi.mask = TVIF_PARAM;
    if (!TreeView_GetItem(explorer->tree_view, &tvi)) return false;
    
    const char *old_path = (const char*)tvi.lParam;
    if (!old_path) return false;
    
    // If no new name provided, show rename dialog
    if (!new_name) {
        // Get just the name
        const char *name = strrchr(old_path, '\\');
        if (name) name++;
        else name = old_path;
        
        // In a full implementation, show an edit control
        // For now, use a simple approach
        return false;
    }
    
    return workspace_rename_file(old_path, new_name);
}

bool explorer_delete(Explorer *explorer, HTREEITEM item) {
    if (!explorer || !item) return false;
    
    TVITEM tvi = {0};
    tvi.hItem = item;
    tvi.mask = TVIF_PARAM;
    if (!TreeView_GetItem(explorer->tree_view, &tvi)) return false;
    
    const char *path = (const char*)tvi.lParam;
    if (!path) return false;
    
    if (workspace_delete_file(path, explorer->hwnd)) {
        explorer_refresh(explorer);
        return true;
    }
    
    return false;
}

bool explorer_duplicate(Explorer *explorer, HTREEITEM item) {
    if (!explorer || !item) return false;
    
    TVITEM tvi = {0};
    tvi.hItem = item;
    tvi.mask = TVIF_PARAM;
    if (!TreeView_GetItem(explorer->tree_view, &tvi)) return false;
    
    const char *path = (const char*)tvi.lParam;
    if (!path) return false;
    
    char new_path[MAX_PATH];
    if (workspace_duplicate_file(path, new_path, sizeof(new_path))) {
        explorer_refresh(explorer);
        return true;
    }
    
    return false;
}

void explorer_reveal(Explorer *explorer, HTREEITEM item) {
    if (!explorer) return;
    
    if (item) {
        TVITEM tvi = {0};
        tvi.hItem = item;
        tvi.mask = TVIF_PARAM;
        if (TreeView_GetItem(explorer->tree_view, &tvi)) {
            const char *path = (const char*)tvi.lParam;
            if (path) {
                workspace_reveal_in_explorer(path);
            }
        }
    } else if (explorer->root_path[0]) {
        workspace_reveal_in_explorer(explorer->root_path);
    }
}