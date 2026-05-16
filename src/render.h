#ifndef RENDER_H
#define RENDER_H

#include "types.h"

#define RENDER_MARGIN_WIDTH 50
#define RENDER_LINE_NUMBER_GUTTER_WIDTH 50

// Initialize rendering resources
bool render_init(Editor *editor);

// Free rendering resources
void render_free(Editor *editor);

// Calculate metrics (char width, line height, etc.)
void render_calc_metrics(Editor *editor, HDC hdc);

// Paint the editor client area
void render_paint(Editor *editor, HDC hdc, const RECT *update_rect, int tab_bar_offset);

// Get line number from y-coordinate
int render_y_to_line(Editor *editor, int y);

// Get y-coordinate from line number
int render_line_to_y(Editor *editor, int line);

// Update viewport dimensions on resize
void render_resize(Editor *editor, int width, int height);

// Get caret display column (tabs expanded to spaces)
int get_caret_display_col(Editor *editor);

// Toggle line numbers
void render_set_show_line_numbers(Editor *editor, bool show);
bool render_get_show_line_numbers(Editor *editor);

// Set search highlight ranges
void render_set_search_highlights(Editor *editor, TextPos *positions, int count);
void render_clear_search_highlights(Editor *editor);

// Bracket highlighting
void render_draw_bracket_highlight(Editor *editor, HDC hdc);

// Auto-complete popup
void render_draw_autocomplete_popup(Editor *editor, HDC hdc);

#endif // RENDER_H
