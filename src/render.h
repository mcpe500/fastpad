#ifndef RENDER_H
#define RENDER_H

#include "types.h"

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

#endif // RENDER_H
