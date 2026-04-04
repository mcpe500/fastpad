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

#endif // APP_H
