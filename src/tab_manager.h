#ifndef TAB_MANAGER_H
#define TAB_MANAGER_H

#include "types.h"

// Initialize tab manager
bool tab_manager_init(TabManager *mgr, HWND parent_hwnd);

// Free tab manager resources (call during normal shutdown, no prompts)
void tab_manager_free(TabManager *mgr);

// Create a new tab (returns tab index, -1 on failure)
int tab_manager_new_tab(TabManager *mgr);

// Close a tab (with optional shutdown mode to prevent auto-creating tabs)
bool tab_manager_close_tab(TabManager *mgr, int index, bool is_shutting_down);

// Switch to a tab
bool tab_manager_switch_tab(TabManager *mgr, int index);

// Get active tab
Tab* tab_manager_get_active(TabManager *mgr);

// Get tab by index
Tab* tab_manager_get_tab(TabManager *mgr, int index);

// Update tab titles and control
void tab_manager_update_control(TabManager *mgr);

// Get tab count
int tab_manager_count(TabManager *mgr);

// Get active tab index
int tab_manager_active_index(TabManager *mgr);

// Handle tab selection from tab control
void tab_manager_on_tab_click(TabManager *mgr, int clicked_index);

// Resize tab bar
void tab_manager_resize(TabManager *mgr, int width);

// Check if any tab has unsaved changes
bool tab_manager_has_unsaved_changes(TabManager *mgr);

// Prompt to save all unsaved tabs
bool tab_manager_check_unsaved(TabManager *mgr);

// Set tab filename and update title
void tab_manager_set_tab_filename(TabManager *mgr, int index, const char *filename);

// Update window title based on active tab
void tab_manager_update_window_title(TabManager *mgr, HWND hwnd);

// Update status bar based on active tab
void tab_manager_update_statusbar(TabManager *mgr);

#endif // TAB_MANAGER_H
