#include "render.h"
#include "buffer.h"
#include "types.h"          /* for extern App g_app */
#include "theme.h"
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>        /* for SIZE struct + GDI functions */

#define FONT_HEIGHT 16
#define FONT_WIDTH 8
#define TAB_SPACES 4  // Number of spaces per tab character

// Helper: find the best wrap point in a string
static int find_wrap_point(const char *text, int max_cols) {
    if (max_cols <= 0) return 0;
    if ((int)strlen(text) <= max_cols) return (int)strlen(text);

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
static char *expand_tabs(const char *text, int text_length, int *out_display_length) {
    int display_len = 0;
    for (int i = 0; i < text_length; i++) {
        if (text[i] == '\t') {
            display_len += TAB_SPACES;
        } else {
            display_len++;
        }
    }

    char *expanded = (char *)malloc(display_len + 1);
    if (!expanded) {
        *out_display_length = text_length;
        return NULL;
    }

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

int get_caret_display_col(Editor *editor) {
    LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
    TextPos line_start = buffer_line_start(&editor->buffer, lc.line);
    return byte_col_to_display_col(&editor->buffer, line_start, editor->caret);
}

bool render_init(Editor *editor) {
    FontSettings *fs = &g_app.font_settings;
    editor->font = CreateFontA(
        fs->font_size, FONT_WIDTH, 0, 0, fs->bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, fs->font_family
    );
    if (!editor->font) editor->font = GetStockObject(SYSTEM_FIXED_FONT);
    
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
    // Subtract margin from visible columns
    editor->viewport.visible_cols = (width - RENDER_MARGIN_WIDTH) / editor->viewport.char_width;
    editor->viewport.visible_lines = height / editor->viewport.line_height;

    int total_lines = buffer_line_count(&editor->buffer);
    if (editor->viewport.scroll_y + editor->viewport.visible_lines > total_lines) {
        editor->viewport.scroll_y = total_lines - editor->viewport.visible_lines;
        if (editor->viewport.scroll_y < 0) editor->viewport.scroll_y = 0;
    }
}

void render_paint(Editor *editor, HDC hdc, const RECT *update_rect, int tab_bar_offset) {
    (void)update_rect;
    RECT client_rect;
    GetClientRect(editor->hwnd, &client_rect);
    int width = client_rect.right;
    int height = client_rect.bottom;

    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP mem_bm = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP old_bm = (HBITMAP)SelectObject(mem_dc, mem_bm);
    HFONT old_font = (HFONT)SelectObject(mem_dc, editor->font);

    HBRUSH bg_brush = CreateSolidBrush(g_app.current_theme->colors.editor_bg);
    FillRect(mem_dc, &client_rect, bg_brush);
    DeleteObject(bg_brush);

    int tab_height = tab_bar_offset;
    bool wrap_enabled = g_app.word_wrap;
    int total_logical_lines = buffer_line_count(&editor->buffer);
    int current_visual_line = 0;
    int logical_line = 0;

    // FIX BUG: Initial scroll position calculation used > instead of >=
    // This caused the first visible line to sometimes not be rendered when
    // scroll_y exactly matched a line boundary
    while (logical_line < total_logical_lines) {
        TextPos l_start = buffer_line_start(&editor->buffer, logical_line);
        TextPos l_end = buffer_line_end(&editor->buffer, logical_line);
        int l_len = l_end - l_start;
        int visual_lines_for_this_logical = 1;
        if (wrap_enabled && l_len > editor->viewport.visible_cols) {
            visual_lines_for_this_logical = (l_len + editor->viewport.visible_cols - 1) / editor->viewport.visible_cols;
        }
        if (current_visual_line + visual_lines_for_this_logical > editor->viewport.scroll_y) break;
        if (current_visual_line + visual_lines_for_this_logical == editor->viewport.scroll_y) {
            // Exactly at scroll boundary - this is the first visible line
            break;
        }
        current_visual_line += visual_lines_for_this_logical;
        logical_line++;
    }

    while (logical_line < total_logical_lines && 
           current_visual_line < editor->viewport.scroll_y + editor->viewport.visible_lines) {
        
        TextPos l_start = buffer_line_start(&editor->buffer, logical_line);
        TextPos l_end = buffer_line_end(&editor->buffer, logical_line);
        int l_len = l_end - l_start;

        char *line_text = buffer_get_text(&editor->buffer, l_start, l_end);
        if (!line_text) {
            logical_line++;
            current_visual_line++; 
            continue;
        }

        int display_length = l_len;
        char *display_text = expand_tabs(line_text, l_len, &display_length);
        if (!display_text) {
            display_text = line_text;
            display_length = l_len;
        }

        TextPos sel_start = editor->selection.start;
        TextPos sel_end = editor->selection.end;
        if (sel_start > sel_end) {
            TextPos temp = sel_start;
            sel_start = sel_end;
            sel_end = temp;
        }
        bool has_selection = sel_start < sel_end;
        
        int chars_processed = 0;
        while (chars_processed < display_length) {
            int visual_line_y = tab_height + (current_visual_line - editor->viewport.scroll_y) * editor->viewport.line_height;
            
            if (visual_line_y >= 0 && visual_line_y < height) {
                // 1. Draw Margin Background
                HBRUSH margin_brush = CreateSolidBrush(g_app.current_theme->colors.margin_bg);
                RECT margin_rect = { 0, visual_line_y, RENDER_MARGIN_WIDTH, visual_line_y + editor->viewport.line_height };
                FillRect(mem_dc, &margin_rect, margin_brush);
                DeleteObject(margin_brush);

                // 2. Draw Line Number
                char line_num_str[32];
                snprintf(line_num_str, sizeof(line_num_str), "%d", logical_line + 1);   // 1-based

                /* FIXED: GetTextExtentPoint32A was called with NULL → undefined behavior / crash */
                SIZE text_size = {0};
                GetTextExtentPoint32A(mem_dc, line_num_str, (int)strlen(line_num_str), &text_size);

                SetTextColor(mem_dc, g_app.current_theme->colors.margin_text);   /* slightly darker for better visibility */
                SetBkMode(mem_dc, TRANSPARENT);
                
                int num_width = text_size.cx;
                
                TextOutA(mem_dc, RENDER_MARGIN_WIDTH - num_width - 5, visual_line_y, line_num_str, (int)strlen(line_num_str));

                // 3. Draw Text
                int remaining = display_length - chars_processed;
                int segment_len = remaining;
                if (wrap_enabled && remaining > editor->viewport.visible_cols) {
                    segment_len = find_wrap_point(display_text + chars_processed, editor->viewport.visible_cols);
                }

                // FIX BUG: Selection rendering in wrapped lines did not account for
                // which visual segment we're in. sel_start_disp/sel_end_disp are
                // global to the line but we need segment-relative values.
                int seg_start_disp = chars_processed;  // display col where this segment starts
                int seg_end_disp = chars_processed + segment_len;  // where it ends
                
                int sel_start_disp = -1, sel_end_disp = -1;
                if (has_selection && sel_start < l_end && sel_end > l_start) {
                    TextPos s_start_in_line = (sel_start > l_start) ? sel_start : l_start;
                    TextPos s_end_in_line = (sel_end < l_end) ? sel_end : l_end;
                    sel_start_disp = byte_col_to_display_col(&editor->buffer, l_start, s_start_in_line);
                    sel_end_disp = byte_col_to_display_col(&editor->buffer, l_start, s_end_in_line);
                }

                if (has_selection && sel_start_disp != -1) {
                    // Calculate intersection with current segment
                    int intersect_start = (seg_start_disp > sel_start_disp) ? seg_start_disp : sel_start_disp;
                    int intersect_end = (seg_end_disp < sel_end_disp) ? seg_end_disp : sel_end_disp;
                    if (intersect_end > intersect_start) {
                        int x1 = RENDER_MARGIN_WIDTH + (intersect_start - editor->viewport.scroll_x) * editor->viewport.char_width;
                        int x2 = RENDER_MARGIN_WIDTH + (intersect_end - editor->viewport.scroll_x) * editor->viewport.char_width;
                        RECT sel_rect = { x1, visual_line_y, x2, visual_line_y + editor->viewport.line_height };
                        HBRUSH sel_brush = CreateSolidBrush(g_app.current_theme->colors.selection_bg);
                        FillRect(mem_dc, &sel_rect, sel_brush);
                        DeleteObject(sel_brush);
                    }
                }

                int x = RENDER_MARGIN_WIDTH - editor->viewport.scroll_x * editor->viewport.char_width;
                
                // FIX BUG: Segments before scroll position have negative x coordinates
                // and would render off-screen. Skip these segments entirely.
                if (x + segment_len * editor->viewport.char_width < RENDER_MARGIN_WIDTH) {
                    chars_processed += segment_len;
                    current_visual_line++;
                    continue;
                }
                
                if (sel_start_disp != -1) {
                    // FIX BUG: Text rendering with selection on wrapped lines
                    // The segment variables were renamed but logic wasn't updated
                    // Now properly uses seg_start_disp/seg_end_disp
                    
                    // Text before selection (unselected portion)
                    int before_sel_len = 0;
                    if (seg_start_disp < sel_start_disp) {
                        before_sel_len = (sel_start_disp < seg_end_disp) ? 
                            (sel_start_disp - seg_start_disp) : segment_len;
                    }
                    
                    if (before_sel_len > 0) {
                        SetTextColor(mem_dc, g_app.current_theme->colors.editor_text);
                        SetBkMode(mem_dc, TRANSPARENT);
                        TextOutA(mem_dc, x, visual_line_y, display_text + chars_processed, before_sel_len);
                    }
                    
                    // Selected portion within this segment
                    int sel_start_in_seg = (sel_start_disp > seg_start_disp) ? 
                        (sel_start_disp - seg_start_disp) : 0;
                    int sel_end_in_seg = (sel_end_disp < seg_end_disp) ? 
                        (sel_end_disp - seg_start_disp) : segment_len;
                    int sel_len_in_seg = sel_end_in_seg - sel_start_in_seg;
                    
                    if (sel_len_in_seg > 0) {
                        SetTextColor(mem_dc, g_app.current_theme->colors.selection_text);
                        SetBkMode(mem_dc, TRANSPARENT);
                        int sel_x = x + sel_start_in_seg * editor->viewport.char_width;
                        TextOutA(mem_dc, sel_x, visual_line_y, display_text + chars_processed + sel_start_in_seg, sel_len_in_seg);
                    }
                    
                    // Text after selection (unselected portion)
                    int after_start_in_seg = (sel_end_disp > seg_start_disp) ? 
                        (sel_end_disp - seg_start_disp) : 0;
                    if (after_start_in_seg < segment_len) {
                        SetTextColor(mem_dc, g_app.current_theme->colors.editor_text);
                        SetBkMode(mem_dc, TRANSPARENT);
                        int after_len = segment_len - after_start_in_seg;
                        int after_x = x + after_start_in_seg * editor->viewport.char_width;
                        TextOutA(mem_dc, after_x, visual_line_y, display_text + chars_processed + after_start_in_seg, after_len);
                    }
                } else {
                    SetTextColor(mem_dc, g_app.current_theme->colors.editor_text);
                    SetBkMode(mem_dc, TRANSPARENT);
                    TextOutA(mem_dc, x, visual_line_y, display_text + chars_processed, segment_len);
                }
                chars_processed += segment_len;
            } else {
                chars_processed = display_length;
            }
            current_visual_line++;
        }
        free(line_text);
        if (display_text != line_text) free(display_text);
        logical_line++;
    }

    if (GetFocus() == editor->hwnd) {
        int caret_display_col = get_caret_display_col(editor);
        LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
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
        if (wrap_enabled) {
            TextPos l_start = buffer_line_start(&editor->buffer, lc.line);
            int col_in_line = byte_col_to_display_col(&editor->buffer, l_start, editor->caret);
            caret_visual_line += col_in_line / editor->viewport.visible_cols;
        }
        // FIX BUG: Caret horizontal position calculation didn't properly handle
        // wrapped lines with horizontal scroll offset
        int caret_x;
        if (wrap_enabled) {
            // For wrapped lines, caret position within the wrapped segment
            // Note: With word wrap enabled, scroll_x is typically 0 since lines wrap
            // But we still need to handle edge cases
            int col_in_wrap = caret_display_col % editor->viewport.visible_cols;
            caret_x = RENDER_MARGIN_WIDTH + col_in_wrap * editor->viewport.char_width;
        } else {
            // Non-wrapped: subtract horizontal scroll offset
            caret_x = RENDER_MARGIN_WIDTH + (caret_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
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
    BitBlt(hdc, 0, 0, width, height, mem_dc, 0, 0, SRCCOPY);
    SelectObject(mem_dc, old_font);
    SelectObject(mem_dc, old_bm);
    DeleteObject(mem_bm);
    DeleteDC(mem_dc);
}

int render_y_to_line(Editor *editor, int y) {
    return editor->viewport.scroll_y + (y / editor->viewport.line_height);
}

int render_line_to_y(Editor *editor, int line) {
    return (line - editor->viewport.scroll_y) * editor->viewport.line_height;
}
