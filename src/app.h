#ifndef APP_H
#define APP_H

#include "types.h"

// Initialize application
bool app_init(App *app, HINSTANCE hInstance);

// Free application resources
void app_free(App *app);

// Create main window
HWND app_create_window(App *app, HINSTANCE hInstance);

// Handle file new
void app_file_new(App *app);

// Handle file open
void app_file_open(App *app);

// Handle file save
void app_file_save(App *app);

// Handle file save as
void app_file_save_as(App *app);

// Handle exit
void app_exit(App *app);

// Update window title
void app_update_title(App *app);

// Update status bar
void app_update_statusbar(App *app);

// Check for unsaved changes and prompt user
bool app_check_unsaved(App *app);

// Handle WM_COMMAND
void app_on_command(App *app, WPARAM wParam);

// Handle WM_SIZE
void app_on_size(App *app, int width, int height);

// Handle WM_PAINT
void app_on_paint(App *app);

// Handle WM_CHAR
void app_on_char(App *app, WPARAM wParam);

// Handle WM_KEYDOWN
void app_on_keydown(App *app, WPARAM wParam);

// Handle WM_LBUTTONDOWN
void app_on_lbuttondown(App *app, int x, int y, bool shift);

// Handle WM_MOUSEMOVE
void app_on_mousemove(App *app, int x, int y, bool dragging);

// Handle WM_LBUTTONUP
void app_on_lbuttonup(App *app);

// Handle WM_SETFOCUS
void app_on_setfocus(App *app);

// Handle WM_MOUSEWHEEL for scrolling
void app_on_mousewheel(App *app, int delta);

// Auto save and session management
void app_init_session(App *app);
void app_save_session(App *app);
void app_restore_session(App *app);
void app_on_idle(App *app);
void app_mark_modified(App *app);

// Recent files management
void recent_files_add(App *app, const char *filename);
void recent_files_remove(App *app, const char *filename);
void recent_files_clear(App *app);
const char** app_get_recent_files(App *app, int *count);

// Zoom functions
void app_handle_zoom(App *app, int delta);
void app_reset_zoom(App *app);

// Custom shortcuts
void app_load_shortcuts(App *app);
void app_save_shortcuts(App *app);

// Split view functions
void app_split_toggle(App *app, SplitMode mode);
void app_split_close(App *app);
void app_cycle_split_focus(App *app);

// Syntax highlighting functions
void app_toggle_highlight(App *app);
LanguageType detect_language(const char *filename);
void highlight_line(const char *line, int len, LanguageType lang, HighlightToken *tokens, int *token_count);
COLORREF get_token_color(HighlightTokenType type);

#endif // APP_H
