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

// Show find dialog
void search_show_dialog(HWND parent_hwnd, Editor *editor);

#endif // SEARCH_H
