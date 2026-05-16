#ifndef TYPES_H
#define TYPES_H

#include "core_types.h"
#include "theme.h"
#include <windows.h>

// Maximum recent files
#define MAX_RECENT_FILES 20
#define MAX_TAB_TITLE 256

// Session data structure
typedef struct {
    char filename[MAX_PATH];
    int cursor_pos;
    int scroll_y;
    int scroll_x;
    bool modified;
} SessionTab;

typedef struct {
    SessionTab tabs[20];
    int tab_count;
    int active_index;
} SessionData;

// Editor viewport state
typedef struct {
    int scroll_y;       // vertical scroll offset (in lines)
    int scroll_x;       // horizontal scroll offset (in characters)
    int visible_lines;  // number of visible lines
    int visible_cols;   // number of visible columns
    int line_height;    // height of a line in pixels
    int char_width;     // average character width in pixels
} Viewport;

// Encoding types
typedef enum {
    ENCODING_UTF8,       // UTF-8 without BOM
    ENCODING_UTF8_BOM,   // UTF-8 with BOM
    ENCODING_ANSI,       // ANSI (system default)
    ENCODING_UTF16LE,    // UTF-16 Little Endian
    ENCODING_UTF16BE     // UTF-16 Big Endian
} EncodingType;

// Line ending types
typedef enum {
    LINE_ENDING_CRLF,    // Windows (CRLF)
    LINE_ENDING_LF,      // Unix (LF)
    LINE_ENDING_CR       // Old Mac (CR)
} LineEndingType;

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
    // Line numbers
    bool show_line_numbers;
    // Search highlights
    TextPos *search_matches;
    int search_match_count;
    // Encoding and line ending settings (per-tab)
    EncodingType encoding;       // File encoding for this tab
    LineEndingType line_ending; // Line ending style for this tab
} Editor;

// Maximum number of tabs
#define MAX_TABS 20
#define MAX_TAB_TITLE 256

// Shortcut binding structure
typedef struct {
    int key;             // Virtual key code
    int modifiers;      // Modifier flags (ctrl, shift, alt)
    char action[32];    // Action identifier string
} ShortcutBinding;

// Maximum number of custom shortcuts
#define MAX_SHORTCUTS 50

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

// Split view modes
typedef enum {
    SPLIT_NONE,
    SPLIT_HORIZONTAL,
    SPLIT_VERTICAL
} SplitMode;

// Highlight token types (renamed to avoid conflict with Windows TOKEN_TYPE)
typedef enum {
    HL_TOKEN_NORMAL,
    HL_TOKEN_KEYWORD,
    HL_TOKEN_STRING,
    HL_TOKEN_NUMBER,
    HL_TOKEN_COMMENT,
    HL_TOKEN_TYPE,
    HL_TOKEN_FUNCTION,
    HL_TOKEN_OPERATOR,
    HL_TOKEN_TAG,       // HTML/XML tags
    HL_TOKEN_ATTRIBUTE, // HTML/XML attributes
    HL_TOKEN_PROPERTY   // CSS properties
} HighlightTokenType;

// Syntax highlighting color theme
typedef struct {
    const char *name;
    COLORREF keyword;   // Keywords (if, for, function, etc.)
    COLORREF string;   // String literals
    COLORREF number;   // Numeric literals
    COLORREF comment;  // Comments
    COLORREF type;     // Type names
    COLORREF function;  // Function names
    COLORREF operator_; // Operators
    COLORREF tag;      // HTML/XML tags
    COLORREF attribute;// HTML/XML attributes
    COLORREF property;  // CSS properties
    COLORREF normal;   // Default text color
} HighlightTheme;

// Syntax highlighting token
typedef struct {
    HighlightTokenType type;
    int start;      // Start position in line
    int end;        // End position in line (exclusive)
} HighlightToken;

// Language type for syntax highlighting
typedef enum {
    LANG_NONE,
    LANG_JSON,
    LANG_XML,
    LANG_HTML,
    LANG_CSS,
    LANG_JAVASCRIPT,
    LANG_PYTHON,
    LANG_MARKDOWN,
    LANG_INI,
    LANG_YAML,
    LANG_SQL,
    LANG_C,
    LANG_CPP,
    LANG_JAVA
} LanguageType;

// Application state
typedef struct {
    TabManager tab_mgr;
    bool word_wrap;
    bool show_statusbar;
    bool show_line_numbers;
    bool highlight_enabled;  // Syntax highlighting enabled
    SplitMode split_mode;
    float split_ratio;       // 0.0 to 1.0 for split position
    int active_split;        // Which split has focus (0 or 1)
    HMENU menu;
    HWND statusbar;
    HWND hwnd;
    bool running;
    bool shutting_down;
    Theme *current_theme;    // Current active theme
    FontSettings font_settings; // Font settings for editor
    // Auto-save and session management
    DWORD last_edit_time;      // Timestamp of last edit for auto-save
    bool auto_save_enabled;
    // Recent files
    char recent_files[MAX_RECENT_FILES][MAX_PATH];
    int recent_count;
    char auto_save_dir[MAX_PATH];  // Directory for auto-save backups
    // Zoom (temporary, not saved)
    int zoom_level;              // Zoom level 50-200 (100 = normal)
    // Custom shortcuts
    ShortcutBinding shortcuts[MAX_SHORTCUTS];
    int shortcut_count;
} App;

// Global app instance
extern App g_app;

#endif // TYPES_H