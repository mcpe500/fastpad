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
    TextPos gap_start;  // start of gap
    int gap_length;     // length of gap
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

#endif // CORE_TYPES_H
