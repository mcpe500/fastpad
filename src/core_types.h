#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Position in text (byte offset from start)
typedef int64_t TextPos;

// Line and column position
typedef struct {
    int line;
    int col;
} LineCol;

// Text selection range
typedef struct {
    TextPos start;
    TextPos end;
} Selection;

// Gap buffer for efficient text editing
typedef struct {
    char *data;         // buffer data
    int size;           // current text size (excluding gap)
    int capacity;       // total buffer capacity (including gap)
    int gap_length;     // length of gap
    TextPos gap_start;  // start of gap
} GapBuffer;

// Undo/redo operation types
typedef enum {
    OP_INSERT,
    OP_DELETE,
    OP_REPLACE
} UndoOpType;

// Undo/redo operation
typedef struct {
    UndoOpType type;
    TextPos pos;
    char *text;
    int length;
    int replace_len;
} UndoOp;

// Undo/redo history
typedef struct {
    UndoOp *ops;
    int count;
    int capacity;
    int current;
    int max_ops;
} UndoHistory;

// Multi-cursor for advanced editing
#define MAX_CURSORS 20
typedef struct {
    TextPos position;     // Caret position
    int display_col;     // Visual column
    int line;            // Visual line
} MultiCursor;

// Column selection state
typedef struct {
    bool active;         // Column selection mode active
    TextPos start;      // Start position
    TextPos end;        // End position (current mouse)
    int start_col;      // Start column
    int end_col;        // End column (current)
} ColumnSelection;

// Code folding
#define MAX_FOLDS 100
typedef struct {
    int start_line;      // First line of foldable block
    int end_line;        // Last line of foldable block
    bool collapsed;      // Is currently collapsed
    char type;           // 'j' = JSON, 'c' = code, 'm' = markdown
} FoldRegion;

// Auto-complete suggestion
#define MAX_SUGGESTIONS 20
#define MAX_AUTOCOMPLETE_WORDS 500
typedef struct {
    char word[64];
    int frequency;
} AutocompleteWord;

typedef struct {
    AutocompleteWord words[MAX_AUTOCOMPLETE_WORDS];
    int count;
    int selected_index;
    bool visible;
    TextPos trigger_pos;  // Position where autocomplete was triggered
} Autocomplete;

#endif // CORE_TYPES_H
