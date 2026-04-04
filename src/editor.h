#ifndef EDITOR_H
#define EDITOR_H

#include "types.h"

// Initialize editor state
bool editor_init(Editor *editor, HWND hwnd);

// Free editor resources
void editor_free(Editor *editor);

// Handle character input
void editor_char_input(Editor *editor, char ch);

// Handle special keys
void editor_key_down(Editor *editor, int key);

// Handle mouse click for caret placement
void editor_mouse_click(Editor *editor, int x, int y, bool shift);

// Handle mouse drag for selection
void editor_mouse_drag(Editor *editor, int x, int y);

// Copy selection to clipboard
bool editor_copy(Editor *editor);

// Cut selection to clipboard
bool editor_cut(Editor *editor);

// Paste from clipboard
bool editor_paste(Editor *editor);

// Select all text
void editor_select_all(Editor *editor);

// Clear selection
void editor_clear_selection(Editor *editor);

// Check if there's a selection
bool editor_has_selection(Editor *editor);

// Get selection bounds (normalized: start <= end)
void editor_get_selection(Editor *editor, TextPos *out_start, TextPos *out_end);

// Update modified state
void editor_set_modified(Editor *editor, bool modified);

// Get current line and column for status bar
void editor_get_linecol(Editor *editor, int *line, int *col);

// Scroll to make caret visible
void editor_scroll_to_caret(Editor *editor);

// Handle window resize
void editor_resize(Editor *editor);

// Undo/Redo
bool editor_undo(Editor *editor);
bool editor_redo(Editor *editor);

#endif // EDITOR_H
