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

// Get word count
int editor_get_word_count(Editor *editor);

// Get character count
int editor_get_char_count(Editor *editor);

// Scroll to make caret visible
void editor_scroll_to_caret(Editor *editor);

// Handle window resize
void editor_resize(Editor *editor);

// Undo/Redo
bool editor_undo(Editor *editor);
bool editor_redo(Editor *editor);

// History persistence
bool editor_save_history(Editor *editor, const char *filename);
bool editor_load_history(Editor *editor, const char *filename);

// Multi-cursor editing
void editor_add_cursor(Editor *editor, int x, int y);
void editor_add_cursor_at_line(Editor *editor, int line, int col);
void editor_select_next_occurrence(Editor *editor);
void editor_remove_cursor(Editor *editor, int index);
void editor_clear_extra_cursors(Editor *editor);
bool editor_has_multiple_cursors(Editor *editor);

// Column selection
void editor_start_column_selection(Editor *editor, int x, int y);
void editor_update_column_selection(Editor *editor, int x, int y);
void editor_end_column_selection(Editor *editor);
bool editor_is_column_selection_active(Editor *editor);

// Bracket highlighting
void editor_update_bracket_match(Editor *editor);
TextPos editor_get_bracket_match(Editor *editor);
int editor_get_bracket_type(Editor *editor);

// Code folding
void editor_detect_folds(Editor *editor);
void editor_toggle_fold(Editor *editor, int line);
bool editor_is_line_folded(Editor *editor, int line);
int editor_get_fold_end_line(Editor *editor, int line);

// Auto-complete
void editor_update_autocomplete(Editor *editor);
void editor_show_autocomplete(Editor *editor);
void editor_hide_autocomplete(Editor *editor);
bool editor_autocomplete_is_visible(Editor *editor);
void editor_autocomplete_select(Editor *editor, int index);
void editor_autocomplete_nav_next(Editor *editor);
void editor_autocomplete_nav_prev(Editor *editor);
const char* editor_autocomplete_get_selected(Editor *editor);

// Bracket auto-close
void editor_auto_close_bracket(Editor *editor, char bracket);

#endif // EDITOR_H
