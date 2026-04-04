#include "tab_manager.h"
#include "editor.h"
#include "render.h"
#include "fileio.h"
#include <stdio.h>
#include <string.h>
#include <commctrl.h>

#define TAB_BAR_HEIGHT 30

bool tab_manager_init(TabManager *mgr, HWND parent_hwnd) {
    memset(mgr, 0, sizeof(TabManager));
    mgr->parent_hwnd = parent_hwnd;
    mgr->active_index = -1;
    mgr->height = TAB_BAR_HEIGHT;
    mgr->next_untitled_id = 1;
    
    // Create tab control
    INITCOMMONCONTROLSEX icex = {0};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);
    
    mgr->hwnd = CreateWindowExA(
        0,
        WC_TABCONTROLA,
        "",
        WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TCS_TOOLTIPS | TCS_SINGLELINE,
        0, 0, 0, 0,
        parent_hwnd,
        NULL,
        NULL,
        NULL
    );
    
    if (!mgr->hwnd) {
        return false;
    }
    
    // Create first tab
    if (tab_manager_new_tab(mgr) == -1) {
        return false;
    }
    
    return true;
}

void tab_manager_free(TabManager *mgr) {
    // Free all tab editor resources directly without prompts or switching
    for (int i = 0; i < mgr->count; i++) {
        editor_free(&mgr->tabs[i].editor);
    }
    mgr->count = 0;
    mgr->active_index = -1;

    if (mgr->hwnd) {
        DestroyWindow(mgr->hwnd);
        mgr->hwnd = NULL;
    }
}

int tab_manager_new_tab(TabManager *mgr) {
    if (mgr->count >= MAX_TABS) {
        MessageBoxA(mgr->parent_hwnd, 
            "Maximum number of tabs reached.",
            "FastPad",
            MB_ICONINFORMATION);
        return -1;
    }
    
    int index = mgr->count;
    Tab *tab = &mgr->tabs[index];
    
    // Initialize tab
    memset(tab, 0, sizeof(Tab));
    tab->active = false;
    snprintf(tab->title, sizeof(tab->title), "Untitled");
    snprintf(tab->filename, sizeof(tab->filename), "Untitled");
    tab->editor.tab_index = index;
    
    // Don't create child window - use parent window directly
    // This ensures keyboard input goes to main window
    tab->hwnd = mgr->parent_hwnd;
    
    // Initialize editor for this tab
    if (!editor_init(&tab->editor, tab->hwnd)) {
        return -1;
    }
    
    render_init(&tab->editor);
    
    // Add to tab control
    TCITEMA tie = {0};
    tie.mask = TCIF_TEXT;
    tie.pszText = tab->title;
    
    if (TabCtrl_InsertItem(mgr->hwnd, index, &tie) == -1) {
        editor_free(&tab->editor);
        DestroyWindow(tab->hwnd);
        return -1;
    }
    
    mgr->count++;
    
    // Switch to this new tab
    tab_manager_switch_tab(mgr, index);
    
    return index;
}

bool tab_manager_close_tab(TabManager *mgr, int index, bool is_shutting_down) {
    if (index < 0 || index >= mgr->count) {
        return false;
    }

    Tab *tab = &mgr->tabs[index];

    // Check for unsaved changes (skip during shutdown)
    if (!is_shutting_down && tab->editor.modified) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Save changes to \"%s\"?", tab->title);

        int result = MessageBoxA(mgr->parent_hwnd, msg, "FastPad",
                                MB_YESNOCANCEL | MB_ICONQUESTION);

        if (result == IDYES) {
            // Save before closing
            if (strcmp(tab->filename, "Untitled") == 0) {
                char filename[MAX_PATH] = {0};
                if (!file_save_dialog(mgr->parent_hwnd, filename, MAX_PATH, &tab->editor.buffer)) {
                    return false; // User cancelled
                }
                tab_manager_set_tab_filename(mgr, index, filename);
            }

            if (!file_save(mgr->parent_hwnd, tab->filename, &tab->editor.buffer)) {
                return false; // Save failed
            }
        } else if (result == IDCANCEL) {
            return false; // User cancelled
        }
        // IDNO: Don't save, just close
    }

    // Free editor resources
    editor_free(&tab->editor);

    // No child window to destroy (using parent directly)
    tab->hwnd = NULL;

    // Remove from tab control
    TabCtrl_DeleteItem(mgr->hwnd, index);

    // Shift remaining tabs
    for (int i = index + 1; i < mgr->count; i++) {
        mgr->tabs[i - 1] = mgr->tabs[i];
        mgr->tabs[i - 1].editor.tab_index = i - 1;
    }

    mgr->count--;

    // Adjust active index
    if (mgr->active_index >= mgr->count) {
        mgr->active_index = mgr->count - 1;
    }

    // If no tabs left, create a new one (only if not shutting down)
    if (mgr->count == 0) {
        if (!is_shutting_down) {
            tab_manager_new_tab(mgr);
        }
        return true;
    }

    // Switch to the tab that replaced the closed one
    if (mgr->active_index >= 0) {
        tab_manager_switch_tab(mgr, mgr->active_index);
    }

    return true;
}

bool tab_manager_switch_tab(TabManager *mgr, int index) {
    if (index < 0 || index >= mgr->count) {
        return false;
    }

    // Deactivate current tab
    if (mgr->active_index >= 0 && mgr->active_index < mgr->count) {
        mgr->tabs[mgr->active_index].active = false;
    }

    // Activate new tab
    mgr->active_index = index;
    mgr->tabs[index].active = true;

    // Update tab control selection
    TabCtrl_SetCurSel(mgr->hwnd, index);

    // Set focus to parent window so keyboard input works
    SetFocus(mgr->parent_hwnd);

    // Hide caret during switch to prevent stale positioning
    HideCaret(mgr->parent_hwnd);

    // Update window title
    tab_manager_update_window_title(mgr, mgr->parent_hwnd);

    // Update status bar
    tab_manager_update_statusbar(mgr);

    // Redraw entire window to ensure clean state
    InvalidateRect(mgr->parent_hwnd, NULL, TRUE);

    return true;
}

Tab* tab_manager_get_active(TabManager *mgr) {
    if (mgr->active_index < 0 || mgr->active_index >= mgr->count) {
        return NULL;
    }
    return &mgr->tabs[mgr->active_index];
}

Tab* tab_manager_get_tab(TabManager *mgr, int index) {
    if (index < 0 || index >= mgr->count) {
        return NULL;
    }
    return &mgr->tabs[index];
}

void tab_manager_update_control(TabManager *mgr) {
    for (int i = 0; i < mgr->count; i++) {
        TCITEMA tie = {0};
        tie.mask = TCIF_TEXT;
        tie.pszText = mgr->tabs[i].title;
        TabCtrl_SetItem(mgr->hwnd, i, &tie);
    }
}

int tab_manager_count(TabManager *mgr) {
    return mgr->count;
}

int tab_manager_active_index(TabManager *mgr) {
    return mgr->active_index;
}

void tab_manager_on_tab_click(TabManager *mgr, int clicked_index) {
    if (clicked_index >= 0 && clicked_index < mgr->count) {
        tab_manager_switch_tab(mgr, clicked_index);
    }
}

void tab_manager_resize(TabManager *mgr, int width) {
    if (!mgr->hwnd) return;
    
    RECT rc;
    GetClientRect(mgr->parent_hwnd, &rc);
    
    // Resize tab control to fill width
    SetWindowPos(mgr->hwnd, NULL,
                 0, 0,
                 width, mgr->height,
                 SWP_NOZORDER);
    
    // Resize active tab's editor area
    Tab *active = tab_manager_get_active(mgr);
    if (active) {
        int tab_top = mgr->height;
        int editor_height = rc.bottom - tab_top;
        
        extern App g_app;
        if (g_app.statusbar && g_app.show_statusbar) {
            RECT sb_rect;
            GetWindowRect(g_app.statusbar, &sb_rect);
            editor_height -= (sb_rect.bottom - sb_rect.top);
        }
        
        render_resize(&active->editor, width, editor_height);
        InvalidateRect(mgr->parent_hwnd, NULL, TRUE);
    }
}

bool tab_manager_has_unsaved_changes(TabManager *mgr) {
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->tabs[i].editor.modified) {
            return true;
        }
    }
    return false;
}

bool tab_manager_check_unsaved(TabManager *mgr) {
    bool has_unsaved = false;

    // Find tabs with unsaved changes
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->tabs[i].editor.modified) {
            has_unsaved = true;
            break;
        }
    }

    if (!has_unsaved) {
        return true;
    }

    int result = MessageBoxA(
        mgr->parent_hwnd,
        "Some tabs have unsaved changes. Save all and exit?",
        "FastPad",
        MB_YESNOCANCEL | MB_ICONQUESTION
    );

    if (result == IDYES) {
        // Save all tabs WITHOUT switching tabs
        // Switching during close can cause infinite loops
        for (int i = 0; i < mgr->count; i++) {
            Tab *tab = &mgr->tabs[i];
            if (tab->editor.modified) {
                if (strcmp(tab->filename, "Untitled") == 0) {
                    // Can't show save dialog during close, just return false
                    // User should save manually before closing
                    return false;
                } else {
                    if (!file_save(mgr->parent_hwnd, tab->filename,
                                  &tab->editor.buffer)) {
                        return false;
                    }
                }
            }
        }
        return true;
    } else if (result == IDCANCEL) {
        return false;
    }
    
    // IDNO: Discard all changes
    return true;
}

void tab_manager_set_tab_filename(TabManager *mgr, int index, const char *filename) {
    if (index < 0 || index >= mgr->count) {
        return;
    }
    
    Tab *tab = &mgr->tabs[index];
    strncpy(tab->filename, filename, MAX_PATH - 1);
    tab->filename[MAX_PATH - 1] = '\0';
    
    // Extract basename for title
    const char *basename = strrchr(filename, '\\');
    if (basename) {
        basename++;
    } else {
        basename = filename;
    }
    
    strncpy(tab->title, basename, MAX_TAB_TITLE - 1);
    tab->title[MAX_TAB_TITLE - 1] = '\0';
    
    // Update tab control
    tab_manager_update_control(mgr);
    
    // Update window title if this is active tab
    if (index == mgr->active_index) {
        tab_manager_update_window_title(mgr, mgr->parent_hwnd);
    }
}

void tab_manager_update_window_title(TabManager *mgr, HWND hwnd) {
    Tab *active = tab_manager_get_active(mgr);
    if (!active) return;
    
    char title[MAX_PATH + 50];
    
    if (active->editor.modified) {
        snprintf(title, sizeof(title), "*%s - FastPad", active->title);
    } else {
        snprintf(title, sizeof(title), "%s - FastPad", active->title);
    }
    
    SetWindowTextA(hwnd, title);
}

void tab_manager_update_statusbar(TabManager *mgr) {
    if (!g_app.statusbar || !g_app.show_statusbar) {
        return;
    }
    
    Tab *active = tab_manager_get_active(mgr);
    if (!active) return;
    
    int line, col;
    editor_get_linecol(&active->editor, &line, &col);
    
    char text[100];
    snprintf(text, sizeof(text), "Tab %d/%d  |  Ln %d, Col %d  %s",
             mgr->active_index + 1, mgr->count,
             line, col,
             active->editor.modified ? "| Modified" : "");
    
    int parts[3];
    RECT rect;
    GetClientRect(mgr->parent_hwnd, &rect);
    
    parts[0] = rect.right / 2;
    parts[1] = (rect.right * 3) / 4;
    parts[2] = -1;
    
    SendMessage(g_app.statusbar, SB_SETPARTS, 3, (LPARAM)parts);
    SendMessage(g_app.statusbar, SB_SETTEXTA, 0, (LPARAM)text);
}
