#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

// Find text in buffer
// Returns position of match, or -1 if not found
TextPos search_find(GapBuffer *buffer, TextPos start_pos, const char *text, bool match_case);

// Find next occurrence (wraps around)
TextPos search_find_next(GapBuffer *buffer, TextPos current_pos, const char *text, bool match_case);

// Find previous occurrence (wraps around)
TextPos search_find_prev(GapBuffer *buffer, TextPos current_pos, const char *text, bool match_case);

// Replace single occurrence and find next
TextPos search_replace_next(GapBuffer *buffer, TextPos current_pos, const char *text, 
                            const char *replace_with, bool match_case, TextPos *out_next_pos);

// Replace all occurrences
int search_replace_all(GapBuffer *buffer, const char *text, const char *replace_with, 
                      bool match_case, bool whole_word, bool regex);

// Find all matches and return positions (caller must free result)
TextPos* search_find_all(GapBuffer *buffer, const char *text, bool match_case, 
                         bool whole_word, int *out_count);

// Show find dialog
void search_show_dialog(HWND parent_hwnd, Editor *editor);

// Show replace dialog
void search_show_replace_dialog(HWND parent_hwnd, Editor *editor);

// JSON formatting functions
bool json_format(GapBuffer *buffer, char **out_result, char **out_error);
bool json_minify(GapBuffer *buffer, char **out_result, char **out_error);
bool json_validate(GapBuffer *buffer, char **out_error, int *out_error_line);

#endif // SEARCH_H
