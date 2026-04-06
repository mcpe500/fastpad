#include "render.h"
#include "buffer.h"
#include <stdlib.h>

#define FONT_HEIGHT 16
#define FONT_WIDTH 8
#define TAB_SPACES 4  // Number of spaces per tab character

// Helper: find the best wrap point in a string
static int find_wrap_point(const char *text, int max_cols) {
    if (max_cols <= 0) return 0;
    if (strlen(text) <= max_cols) return (int)strlen(text);

    // Look for the last space before the limit
    for (int i = max_cols - 1; i >= 0; i--) {
        if (text[i] == ' ') {
            return i;
        }
    }

    // No space found, wrap at the limit
    return max_cols;
}

// Helper: expand tab characters to spaces for display

// Returns newly allocated string with tabs expanded
// Sets *out_display_length to the display length
static char *expand_tabs(const char *text, int text_length, int *out_display_length) {
    // First pass: count display length
    int display_len = 0;
    for (int i = 0; i < text_length; i++) {
        if (text[i] == '\t') {
            display_len += TAB_SPACES;
        } else {
            display_len++;
        }
    }

    // Allocate expanded text
    char *expanded = (char *)malloc(display_len + 1);
    if (!expanded) {
        *out_display_length = text_length;
        return NULL;
    }

    // Second pass: expand tabs
    int j = 0;
    for (int i = 0; i < text_length; i++) {
        if (text[i] == '\t') {
            for (int s = 0; s < TAB_SPACES; s++) {
                expanded[j++] = ' ';
            }
        } else {
            expanded[j++] = text[i];
        }
    }
    expanded[j] = '\0';
    *out_display_length = display_len;
    return expanded;
}

// Helper: calculate visual column of a byte position (expanding tabs)
static int byte_col_to_display_col(GapBuffer *buf, TextPos byte_start, TextPos byte_pos) {
    int display_col = 0;
    for (TextPos i = byte_start; i < byte_pos && i < byte_start + 10000; i++) {
        char c = buffer_get_char(buf, i);
        if (c == '\t') {
            display_col += TAB_SPACES;
        } else {
            display_col++;
        }
    }
    return display_col;
}

// Helper: get the display column of caret within its line (NOT static - used by editor_scroll_to_caret)
int get_caret_display_col(Editor *editor) {
    LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
    TextPos line_start = buffer_line_start(&editor->buffer, lc.line);
    return byte_col_to_display_col(&editor->buffer, line_start, editor->caret);
}

bool render_init(Editor *editor) {
    // Create monospace font
    editor->font = CreateFontA(
        FONT_HEIGHT,
        FONT_WIDTH,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        FIXED_PITCH | FF_DONTCARE,
        "Consolas"
    );
    
    if (!editor->font) {
        // Fallback to system font
        editor->font = GetStockObject(SYSTEM_FIXED_FONT);
    }
    
    editor->font_height = FONT_HEIGHT;
    editor->char_width = FONT_WIDTH;
    editor->viewport.scroll_x = 0;
    editor->viewport.scroll_y = 0;
    editor->viewport.line_height = FONT_HEIGHT;
    editor->viewport.char_width = FONT_WIDTH;
    
    return true;
}

void render_free(Editor *editor) {
    if (editor->font) {
        DeleteObject(editor->font);
        editor->font = NULL;
    }
}

void render_calc_metrics(Editor *editor, HDC hdc) {
    HFONT old_font = (HFONT)SelectObject(hdc, editor->font);
    
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    
    editor->font_height = tm.tmHeight;
    editor->char_width = tm.tmAveCharWidth;
    editor->viewport.line_height = tm.tmHeight;
    editor->viewport.char_width = tm.tmAveCharWidth;
    
    SelectObject(hdc, old_font);
}

void render_resize(Editor *editor, int width, int height) {
    editor->viewport.visible_lines = height / editor->viewport.line_height;
    editor->viewport.visible_cols = width / editor->viewport.char_width;

    // Clamp scroll position
    int total_lines = buffer_line_count(&editor->buffer);
    if (editor->viewport.scroll_y + editor->viewport.visible_lines > total_lines) {
        editor->viewport.scroll_y = total_lines - editor->viewport.visible_lines;
        if (editor->viewport.scroll_y < 0) {
            editor->viewport.scroll_y = 0;
        }
    }
}

void render_paint(Editor *editor, HDC hdc, const RECT *update_rect, int tab_bar_offset) {
    (void)update_rect;

    // Select font
    HFONT old_font = (HFONT)SelectObject(hdc, editor->font);

    // Get client area
    RECT client_rect;
    GetClientRect(editor->hwnd, &client_rect);

    // Fill ENTIRE client area with white background first
    HBRUSH bg_brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &client_rect, bg_brush);
    DeleteObject(bg_brush);

    // Adjust client rect for tab bar
    client_rect.top = tab_bar_offset;
    int tab_height = tab_bar_offset;

    // Word wrap state
    bool wrap_enabled = g_app.word_wrap;

    // We need to map viewport.scroll_y (visual lines) to logical lines.
    // Since we don't have a cache, we scan from the beginning.
    int total_logical_lines = buffer_line_count(&editor->buffer);
    int current_visual_line = 0;
    int logical_line = 0;

    // 1. Skip logical lines until we reach the first visible visual line
    while (logical_line < total_logical_lines) {
        TextPos l_start = buffer_line_start(&editor->buffer, logical_line);
        TextPos l_end = buffer_line_end(&editor->buffer, logical_line);
        int l_len = l_end - l_start;
        
        int visual_lines_for_this_logical = 1;
        if (wrap_enabled && l_len > editor->viewport.visible_cols) {
            visual_lines_for_this_logical = (l_len + editor->viewport.visible_cols - 1) / editor->viewport.visible_cols;
        }

        if (current_visual_line + visual_lines_for_this_logical > editor->viewport.scroll_y) {
            break; // Found the first visible logical line
        }
        current_visual_line += visual_lines_for_this_logical;
        logical_line++;
    }

    // 2. Render until we fill the visible area
    while (logical_line < total_logical_lines && 
           current_visual_line < editor->viewport.scroll_y + editor->viewport.visible_lines) {
        
        TextPos l_start = buffer_line_start(&editor->buffer, logical_line);
        TextPos l_end = buffer_line_end(&editor->buffer, logical_line);
        int l_len = l_end - l_start;

        char *line_text = buffer_get_text(&editor->buffer, l_start, l_end);
        if (!line_text) {
            logical_line++;
            // Need to account for visual lines even if text is null (though unlikely)
            current_visual_line++; 
            continue;
        }

        // Expand tabs for display
        int display_length = l_len;
        char *display_text = expand_tabs(line_text, l_len, &display_length);
        if (!display_text) {
            display_text = line_text;
            display_length = l_len;
        }

        // Selection logic for this logical line
        TextPos sel_start = editor->selection.start;
        TextPos sel_end = editor->selection.end;
        if (sel_start > sel_end) {
            TextPos temp = sel_start;
            sel_start = sel_end;
            sel_end = temp;
        }

        bool has_selection = sel_start < sel_end;
        
        // Render this logical line as one or more visual lines
        int chars_processed = 0;
        while (chars_processed < display_length) {
            int visual_line_y = tab_height + (current_visual_line - editor->viewport.scroll_y) * editor->viewport.line_height;
            
            // Only draw if it's within the visible viewport
            if (visual_line_y < 0) {
                // This visual segment is above the viewport (happens if the first visible logical line wraps)
            } else if (visual_line_y < client_rect.bottom) {
                int remaining = display_length - chars_processed;
                int segment_len = remaining;
                
                if (wrap_enabled && remaining > editor->viewport.visible_cols) {
                    segment_len = find_wrap_point(display_text + chars_processed, editor->viewport.visible_cols);
                }

                // Selection rendering for this segment
                int seg_start_col = chars_processed;
                int seg_end_col = chars_processed + segment_len;

                // Calculate selection display columns for this logical line
                int sel_start_disp = -1, sel_end_disp = -1;
                if (has_selection && sel_start < l_end && sel_end > l_start) {
                    TextPos s_start_in_line = (sel_start > l_start) ? sel_start : l_start;
                    TextPos s_end_in_line = (sel_end < l_end) ? sel_end : l_end;
                    sel_start_disp = byte_col_to_display_col(&editor->buffer, l_start, s_start_in_line);
                    sel_end_disp = byte_col_to_display_col(&editor->buffer, l_start, s_end_in_line);
                }

                // Draw selection background for this segment
                if (sel_start_disp != -1) {
                    int intersect_start = (seg_start_col > sel_start_disp) ? seg_start_col : sel_start_disp;
                    int intersect_end = (seg_end_col < sel_end_disp) ? seg_end_col : sel_end_disp;
                    
                    if (intersect_end > intersect_start) {
                        int x1 = (intersect_start - editor->viewport.scroll_x) * editor->viewport.char_width;
                        int x2 = (intersect_end - editor->viewport.scroll_x) * editor->viewport.char_width;
                        RECT sel_rect = { x1, visual_line_y, x2, visual_line_y + editor->viewport.line_height };
                        HBRUSH sel_brush = CreateSolidBrush(RGB(0, 120, 215));
                        FillRect(hdc, &sel_rect, sel_brush);
                        DeleteObject(sel_brush);
                    }
                }

                // Draw text for this segment
                int x = -editor->viewport.scroll_x * editor->viewport.char_width;
                
                if (sel_start_disp != -1) {
                    // Segmented drawing for selection
                    int s1 = (seg_start_col < sel_start_disp) ? (sel_start_disp < seg_end_col ? sel_start_disp - seg_start_col : segment_len) : 0;
                    
                    // Part 1: Before selection
                    if (s1 > 0) {
                        SetTextColor(hdc, RGB(0, 0, 0));
                        SetBkMode(hdc, TRANSPARENT);
                        TextOutA(hdc, x, visual_line_y, display_text + chars_processed, s1);
                    }

                    // Part 2: Selection
                    int sel_start_in_seg = (seg_start_col > sel_start_disp) ? seg_start_col - sel_start_disp : 0;
                    int sel_end_in_seg = (seg_end_col < sel_end_disp) ? seg_end_col - sel_start_disp : sel_end_disp - sel_start_disp;
                    
                    int sel_len_in_seg = sel_end_in_seg - sel_start_in_seg;
                    if (sel_len_in_seg > 0) {
                        SetTextColor(hdc, RGB(255, 255, 255));
                        SetBkMode(hdc, TRANSPARENT);
                        int sel_x = x + (seg_start_col + sel_start_in_seg) * editor->viewport.char_width;
                        TextOutA(hdc, sel_x, visual_line_y, display_text + chars_processed + sel_start_in_seg, sel_len_in_seg);
                    }

                    // Part 3: After selection
                    int after_start_in_seg = (seg_end_col > sel_end_disp) ? sel_end_disp - seg_start_col : segment_len;
                    if (after_start_in_seg < segment_len) {
                        SetTextColor(hdc, RGB(0, 0, 0));
                        SetBkMode(hdc, TRANSPARENT);
                        int after_x = x + (seg_start_col + after_start_in_seg) * editor->viewport.char_width;
                        TextOutA(hdc, after_x, visual_line_y, display_text + chars_processed + after_start_in_seg, segment_len - after_start_in_seg);
                    }
                } else {
                    SetTextColor(hdc, RGB(0, 0, 0));
                    SetBkMode(hdc, TRANSPARENT);
                    TextOutA(hdc, x, visual_line_y, display_text + chars_processed, segment_len);
                }
                
                chars_processed += segment_len;
            } else {
                chars_processed = display_length; // Skip if off-screen
            }
            current_visual_line++;
        }

        free(line_text);
        if (display_text != line_text) free(display_text);
        logical_line++;
    }

    // Draw caret
    if (GetFocus() == editor->hwnd) {
        int caret_display_col = get_caret_display_col(editor);
        LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
        
        // Calculate visual line of the caret
        int caret_visual_line = 0;
        for (int l = 0; l < lc.line; l++) {
            TextPos l_start = buffer_line_start(&editor->buffer, l);
            TextPos l_end = buffer_line_end(&editor->buffer, l);
            int l_len = l_end - l_start;
            if (wrap_enabled && l_len > editor->viewport.visible_cols) {
                caret_visual_line += (l_len + editor->viewport.visible_cols - 1) / editor->viewport.visible_cols;
            } else {
                caret_visual_line++;
            }
        }
        
        // Offset within the wrapped line
        if (wrap_enabled) {
            TextPos l_start = buffer_line_start(&editor->buffer, lc.line);
            int col_in_line = byte_col_to_display_col(&editor->buffer, l_start, editor->caret);
            caret_visual_line += col_in_line / editor->viewport.visible_cols;
        }

        int caret_x = (caret_display_col % (wrap_enabled ? editor->viewport.visible_cols : 1000000) - editor->viewport.scroll_x) * editor->viewport.char_width;
        // Wait, caret_display_col is absolute in line. We need relative to the wrap segment.
        if (wrap_enabled) {
            int col_relative = caret_display_col % editor->viewport.visible_cols;
            caret_x = (col_relative - editor->viewport.scroll_x) * editor->viewport.char_width;
        } else {
            caret_x = (caret_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
        }
        
        int caret_y = tab_height + (caret_visual_line - editor->viewport.scroll_y) * editor->viewport.line_height;

        if (caret_visual_line >= editor->viewport.scroll_y && 
            caret_visual_line < editor->viewport.scroll_y + editor->viewport.visible_lines) {
            CreateCaret(editor->hwnd, NULL, 1, editor->viewport.line_height);
            SetCaretPos(caret_x, caret_y);
            ShowCaret(editor->hwnd);
        } else {
            HideCaret(editor->hwnd);
        }
    } else {
        HideCaret(editor->hwnd);
    }
    
    SelectObject(hdc, old_font);
}

int render_y_to_line(Editor *editor, int y) {
    return editor->viewport.scroll_y + (y / editor->viewport.line_height);
}

int render_line_to_y(Editor *editor, int line) {
    return (line - editor->viewport.scroll_y) * editor->viewport.line_height;
}
