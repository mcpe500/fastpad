#ifndef TYPES_H
#define TYPES_H

#include "core_types.h"
#include <windows.h>

// Editor viewport state
typedef struct {
    int scroll_y;       // vertical scroll offset (in lines)
    int scroll_x;       // horizontal scroll offset (in characters)
    int visible_lines;  // number of visible lines
    int visible_cols;   // number of visible columns
    int line_height;    // height of a line in pixels
    int char_width;     // average character width in pixels
} Viewport;

// Editor state (per tab)
typedef struct {
    GapBuffer buffer;
    Selection selection;
    TextPos caret;      // current caret position
    bool modified;      // file has unsaved changes
    int undo_count_at_save;  // undo.current value when last saved (for modified flag tracking)
    UndoHistory undo;
    UndoHistory redo;
    Viewport viewport;
    HWND hwnd;          // editor window handle
    HFONT font;         // monospace font
    int font_height;
    int char_width;
    int tab_index;      // which tab this editor belongs to
} Editor;

// Maximum number of tabs
#define MAX_TABS 20
#define MAX_TAB_TITLE 256

// Tab structure
typedef struct {
    Editor editor;              // Each tab has its own editor
    char filename[MAX_PATH];    // Full path of opened file
    char title[MAX_TAB_TITLE];  // Display title (filename or "Untitled")
    bool active;                // Whether this tab is currently active
    HWND hwnd;                  // Tab-specific child window (for focus handling)
} Tab;

// Tab manager
typedef struct {
    Tab tabs[MAX_TABS];
    int count;                  // Number of open tabs
    int active_index;           // Index of active tab (0-based)
    HWND hwnd;                  // Tab control handle (SysTabControl32)
    HWND parent_hwnd;           // Parent window handle
    int height;                 // Tab bar height in pixels
    int next_untitled_id;       // Counter for "Untitled N" naming
} TabManager;

// Application state
typedef struct {
    TabManager tab_mgr;
    bool word_wrap;
    bool show_statusbar;
    HMENU menu;
    HWND statusbar;
    HWND hwnd;
    bool running;
    bool shutting_down;  // Flag to prevent auto-creating tabs during shutdown
} App;

// Global app instance
extern App g_app;

#endif // TYPES_H
