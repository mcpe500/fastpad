#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <windows.h>

// Position in text (byte offset from start)
typedef int64_t TextPos;

// Line and column position (1-based for display, 0-based internally)
typedef struct {
    int line;   // 0-based line number
    int col;    // 0-based column number (character index within line)
} LineCol;

// Text selection range
typedef struct {
    TextPos start;  // inclusive start position
    TextPos end;    // exclusive end position
} Selection;

// Gap buffer for efficient text editing
typedef struct {
    char *data;         // buffer data
    int size;           // current text size (excluding gap)
    int capacity;       // total buffer capacity (including gap)
    int gap_start;      // start of gap (byte offset from data start)
    int gap_length;     // length of gap
} GapBuffer;

// Undo/redo operation types
typedef enum {
    OP_INSERT,
    OP_DELETE
} UndoOpType;

// Undo/redo operation
typedef struct {
    UndoOpType type;
    TextPos pos;        // position where operation occurred
    char *text;         // inserted or deleted text
    int length;         // length of text
} UndoOp;

// Undo/redo history
typedef struct {
    UndoOp *ops;
    int count;
    int capacity;
    int current;        // current position in history (for redo)
    int max_ops;        // maximum number of operations to keep
} UndoHistory;

// Editor viewport state
typedef struct {
    int scroll_y;       // vertical scroll offset (in lines)
    int scroll_x;       // horizontal scroll offset (in characters)
    int visible_lines;  // number of visible lines
    int visible_cols;   // number of visible columns
    int line_height;    // height of a line in pixels
    int char_width;     // average character width in pixels
} Viewport;

// Editor state
typedef struct {
    GapBuffer buffer;
    Selection selection;
    TextPos caret;      // current caret position
    bool modified;      // file has unsaved changes
    UndoHistory undo;
    UndoHistory redo;
    Viewport viewport;
    HWND hwnd;          // editor window handle
    HFONT font;         // monospace font
    int font_height;
    int char_width;
} Editor;

// Application state
typedef struct {
    Editor editor;
    char filename[MAX_PATH];
    bool word_wrap;
    bool show_statusbar;
    HMENU menu;
    HWND statusbar;
    HWND hwnd;
    bool running;
} App;

// Global app instance
extern App g_app;

#endif // TYPES_H
