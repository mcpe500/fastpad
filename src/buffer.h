#ifndef BUFFER_H
#define BUFFER_H

#include "core_types.h"

// Initialize gap buffer with initial capacity
bool buffer_init(GapBuffer *buf, int initial_capacity);

// Free buffer resources
void buffer_free(GapBuffer *buf);

// Insert text at current caret position
bool buffer_insert(GapBuffer *buf, TextPos pos, const char *text, int length);

// Delete text at position
bool buffer_delete(GapBuffer *buf, TextPos pos, int length);

// Get text content (returns newly allocated string, caller must free)
char *buffer_get_text(GapBuffer *buf, TextPos start, TextPos end);

// Get character at position (returns 0 if out of bounds)
char buffer_get_char(GapBuffer *buf, TextPos pos);

// Get total text length
int buffer_length(GapBuffer *buf);

// Move gap to position (for efficient access)
void buffer_move_gap(GapBuffer *buf, TextPos pos);

// Ensure there's enough gap space
bool buffer_ensure_space(GapBuffer *buf, int needed);

// Convert byte offset to line/column
LineCol buffer_pos_to_linecol(GapBuffer *buf, TextPos pos);

// Convert line/column to byte offset
TextPos buffer_linecol_to_pos(GapBuffer *buf, LineCol lc);

// Get line count
int buffer_line_count(GapBuffer *buf);

// Get line start position
TextPos buffer_line_start(GapBuffer *buf, int line);

// Get line end position (exclusive)
TextPos buffer_line_end(GapBuffer *buf, int line);

// Get line length (excluding line ending)
int buffer_line_length(GapBuffer *buf, int line);

#endif // BUFFER_H
