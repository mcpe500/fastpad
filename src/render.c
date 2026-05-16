#include "render.h"
#include "buffer.h"
#include "types.h"          /* for extern App g_app */
#include "theme.h"
#include "app.h"            /* for g_app, syntax highlighting */
#include "tab_manager.h"    /* for tab_manager_get_active */
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>        /* for SIZE struct + GDI functions */
#include <ctype.h>          /* for isalnum */
#include <string.h>         /* for strcmp */

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

    // Handle split view rendering
    if (g_app.split_mode != SPLIT_NONE) {
        // Calculate split dimensions
        int split1_left, split1_top, split1_width, split1_height;
        int split2_left, split2_top, split2_width, split2_height;
        
        if (g_app.split_mode == SPLIT_HORIZONTAL) {
            // Horizontal split - side by side
            int divider_x = (int)(width * g_app.split_ratio);
            
            // First split (left)
            split1_left = 0;
            split1_top = tab_bar_offset;
            split1_width = divider_x;
            split1_height = height - tab_bar_offset;
            
            // Second split (right)
            split2_left = divider_x + 1;  // +1 for divider line
            split2_top = tab_bar_offset;
            split2_width = width - divider_x - 1;
            split2_height = height - tab_bar_offset;
            
            // Draw divider line
            HBRUSH divider_brush = CreateSolidBrush(g_app.current_theme->colors.margin_bg);
            RECT divider_rect = { divider_x, tab_bar_offset, divider_x + 1, height };
            FillRect(hdc, &divider_rect, divider_brush);
            DeleteObject(divider_brush);
        } else {
            // Vertical split - stacked
            int divider_y = tab_bar_offset + (int)((height - tab_bar_offset) * g_app.split_ratio);
            
            // First split (top)
            split1_left = 0;
            split1_top = tab_bar_offset;
            split1_width = width;
            split1_height = divider_y - tab_bar_offset;
            
            // Second split (bottom)
            split2_left = 0;
            split2_top = divider_y + 1;  // +1 for divider line
            split2_width = width;
            split2_height = height - divider_y - 1;
            
            // Draw divider line
            HBRUSH divider_brush = CreateSolidBrush(g_app.current_theme->colors.margin_bg);
            RECT divider_rect = { 0, divider_y, width, divider_y + 1 };
            FillRect(hdc, &divider_rect, divider_brush);
            DeleteObject(divider_brush);
        }
        
        // Create memory DCs for each split
        HDC mem_dc1 = CreateCompatibleDC(hdc);
        HBITMAP mem_bm1 = CreateCompatibleBitmap(hdc, split1_width, split1_height);
        SelectObject(mem_dc1, mem_bm1);
        
        HDC mem_dc2 = CreateCompatibleDC(hdc);
        HBITMAP mem_bm2 = CreateCompatibleBitmap(hdc, split2_width, split2_height);
        SelectObject(mem_dc2, mem_bm2);
        
        // Paint first split
        BitBlt(mem_dc1, 0, 0, split1_width, split1_height, hdc, 0, tab_bar_offset, SRCCOPY);
        // For first split, we use active_split's scroll or split 0's scroll
        // We'll use the same buffer but different viewports
        
        // For simplicity, render to each split using the same content
        // but potentially different scroll offsets
        // Split 0 uses scroll from editor, Split 1 uses scroll from second viewport stored elsewhere
        
        // For simplicity, both viewports share the same scroll position
        // Each viewport could have its own scroll, but that requires more state
        
        // Render to first split
        {
            HBRUSH bg_brush = CreateSolidBrush(g_app.current_theme->colors.editor_bg);
            RECT rc = {0, 0, split1_width, split1_height};
            FillRect(mem_dc1, &rc, bg_brush);
            DeleteObject(bg_brush);
            
            // Calculate visible lines for this split
            int total_lines = buffer_line_count(&editor->buffer);
            
            // Render content (simplified - same content in both splits)
            int current_visual_line = editor->viewport.scroll_y;
            int logical_line = 0;
            
            // Find starting logical line based on scroll
            int vl_count = 0;
            for (int l = 0; l < total_lines && vl_count < current_visual_line; l++) {
                TextPos l_start = buffer_line_start(&editor->buffer, l);
                TextPos l_end = buffer_line_end(&editor->buffer, l);
                int l_len = l_end - l_start;
                int vis_lines_for_l = 1;
                if (g_app.word_wrap && l_len > editor->viewport.visible_cols) {
                    vis_lines_for_l = (l_len + editor->viewport.visible_cols - 1) / editor->viewport.visible_cols;
                }
                vl_count += vis_lines_for_l;
                logical_line = l + 1;
            }
            
            // Render visible lines
            int y_pos = 0;
            while (logical_line < total_lines && y_pos < split1_height) {
                TextPos l_start = buffer_line_start(&editor->buffer, logical_line);
                TextPos l_end = buffer_line_end(&editor->buffer, logical_line);
                int l_len = l_end - l_start;
                
                char *line_text = buffer_get_text(&editor->buffer, l_start, l_end);
                if (line_text) {
                    int display_length = l_len;
                    char *display_text = expand_tabs(line_text, l_len, &display_length);
                    if (display_text) {
                        // Draw line number gutter
                        HBRUSH margin_brush = CreateSolidBrush(g_app.current_theme->colors.margin_bg);
                        RECT margin_rect = { 0, y_pos, RENDER_MARGIN_WIDTH, y_pos + editor->viewport.line_height };
                        FillRect(mem_dc1, &margin_rect, margin_brush);
                        DeleteObject(margin_brush);
                        
                        char line_num_str[32];
                        snprintf(line_num_str, sizeof(line_num_str), "%d", logical_line + 1);
                        SIZE text_size = {0};
                        GetTextExtentPoint32A(mem_dc1, line_num_str, (int)strlen(line_num_str), &text_size);
                        SetTextColor(mem_dc1, g_app.current_theme->colors.margin_text);
                        SetBkMode(mem_dc1, TRANSPARENT);
                        TextOutA(mem_dc1, RENDER_MARGIN_WIDTH - text_size.cx - 5, y_pos, line_num_str, (int)strlen(line_num_str));
                        
                        // Draw text (simplified without word wrap handling)
                        SetTextColor(mem_dc1, g_app.current_theme->colors.editor_text);
                        SetBkMode(mem_dc1, TRANSPARENT);
                        int text_len = display_length;
                        if (text_len > split1_width / editor->viewport.char_width) {
                            text_len = split1_width / editor->viewport.char_width;
                        }
                        TextOutA(mem_dc1, RENDER_MARGIN_WIDTH, y_pos, display_text, text_len);
                        
                        free(display_text);
                    }
                    free(line_text);
                }
                
                y_pos += editor->viewport.line_height;
                logical_line++;
            }
        }
        
        // Copy first split to screen
        BitBlt(hdc, split1_left, split1_top, split1_width, split1_height, mem_dc1, 0, 0, SRCCOPY);
        
        // Render second split (same content but can have different scroll - using same for now)
        {
            HBRUSH bg_brush = CreateSolidBrush(g_app.current_theme->colors.editor_bg);
            RECT rc = {0, 0, split2_width, split2_height};
            FillRect(mem_dc2, &rc, bg_brush);
            DeleteObject(bg_brush);
            
            int total_lines = buffer_line_count(&editor->buffer);
            int y_pos = 0;
            int logical_line = 0;
            
            // Find starting logical line based on scroll
            int vl_count = 0;
            for (int l = 0; l < total_lines && vl_count < editor->viewport.scroll_y; l++) {
                TextPos l_start = buffer_line_start(&editor->buffer, l);
                TextPos l_end = buffer_line_end(&editor->buffer, l);
                int l_len = l_end - l_start;
                int vis_lines_for_l = 1;
                if (g_app.word_wrap && l_len > editor->viewport.visible_cols) {
                    vis_lines_for_l = (l_len + editor->viewport.visible_cols - 1) / editor->viewport.visible_cols;
                }
                vl_count += vis_lines_for_l;
                logical_line = l + 1;
            }
            
            while (logical_line < total_lines && y_pos < split2_height) {
                TextPos l_start = buffer_line_start(&editor->buffer, logical_line);
                TextPos l_end = buffer_line_end(&editor->buffer, logical_line);
                int l_len = l_end - l_start;
                
                char *line_text = buffer_get_text(&editor->buffer, l_start, l_end);
                if (line_text) {
                    int display_length = l_len;
                    char *display_text = expand_tabs(line_text, l_len, &display_length);
                    if (display_text) {
                        // Draw line number gutter
                        HBRUSH margin_brush = CreateSolidBrush(g_app.current_theme->colors.margin_bg);
                        RECT margin_rect = { 0, y_pos, RENDER_MARGIN_WIDTH, y_pos + editor->viewport.line_height };
                        FillRect(mem_dc2, &margin_rect, margin_brush);
                        DeleteObject(margin_brush);
                        
                        char line_num_str[32];
                        snprintf(line_num_str, sizeof(line_num_str), "%d", logical_line + 1);
                        SIZE text_size = {0};
                        GetTextExtentPoint32A(mem_dc2, line_num_str, (int)strlen(line_num_str), &text_size);
                        SetTextColor(mem_dc2, g_app.current_theme->colors.margin_text);
                        SetBkMode(mem_dc2, TRANSPARENT);
                        TextOutA(mem_dc2, RENDER_MARGIN_WIDTH - text_size.cx - 5, y_pos, line_num_str, (int)strlen(line_num_str));
                        
                        // Draw text
                        SetTextColor(mem_dc2, g_app.current_theme->colors.editor_text);
                        SetBkMode(mem_dc2, TRANSPARENT);
                        int text_len = display_length;
                        if (text_len > split2_width / editor->viewport.char_width) {
                            text_len = split2_width / editor->viewport.char_width;
                        }
                        TextOutA(mem_dc2, RENDER_MARGIN_WIDTH, y_pos, display_text, text_len);
                        
                        free(display_text);
                    }
                    free(line_text);
                }
                
                y_pos += editor->viewport.line_height;
                logical_line++;
            }
        }
        
        // Copy second split to screen
        BitBlt(hdc, split2_left, split2_top, split2_width, split2_height, mem_dc2, 0, 0, SRCCOPY);
        
        // Cleanup
        DeleteObject(mem_bm1);
        DeleteDC(mem_dc1);
        DeleteObject(mem_bm2);
        DeleteDC(mem_dc2);
        
        // Draw caret in focused split if editor has focus
        if (GetFocus() == editor->hwnd) {
            int caret_display_col = get_caret_display_col(editor);
            LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
            
            int caret_visual_line = 0;
            for (int l = 0; l < lc.line; l++) {
                TextPos l_start = buffer_line_start(&editor->buffer, l);
                TextPos l_end = buffer_line_end(&editor->buffer, l);
                int l_len = l_end - l_start;
                if (g_app.word_wrap && l_len > editor->viewport.visible_cols) {
                    caret_visual_line += (l_len + editor->viewport.visible_cols - 1) / editor->viewport.visible_cols;
                } else {
                    caret_visual_line++;
                }
            }
            
            int caret_x, caret_y;
            int split_focus = g_app.active_split;
            
            // Calculate caret position based on which split has focus
            if (g_app.split_mode == SPLIT_HORIZONTAL) {
                int divider_x = (int)(width * g_app.split_ratio);
                if (split_focus == 0) {
                    // Left split
                    caret_x = RENDER_MARGIN_WIDTH + (caret_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
                    caret_y = tab_bar_offset + (caret_visual_line - editor->viewport.scroll_y) * editor->viewport.line_height;
                } else {
                    // Right split
                    caret_x = divider_x + 1 + RENDER_MARGIN_WIDTH + (caret_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
                    caret_y = tab_bar_offset + (caret_visual_line - editor->viewport.scroll_y) * editor->viewport.line_height;
                }
            } else {
                // Vertical split
                int divider_y = tab_bar_offset + (int)((height - tab_bar_offset) * g_app.split_ratio);
                if (split_focus == 0) {
                    // Top split
                    caret_x = RENDER_MARGIN_WIDTH + (caret_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
                    caret_y = tab_bar_offset + (caret_visual_line - editor->viewport.scroll_y) * editor->viewport.line_height;
                } else {
                    // Bottom split
                    caret_x = RENDER_MARGIN_WIDTH + (caret_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
                    caret_y = divider_y + 1 + (caret_visual_line - editor->viewport.scroll_y) * editor->viewport.line_height;
                }
            }
            
            CreateCaret(editor->hwnd, NULL, 1, editor->viewport.line_height);
            SetCaretPos(caret_x, caret_y);
            ShowCaret(editor->hwnd);
        } else {
            HideCaret(editor->hwnd);
        }
        
        return;
    }
    
    // Original single-pane rendering path

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
                    // Syntax highlighting mode - tokenize and render with colors
                    if (g_app.highlight_enabled) {
                        // Get language for current tab
                        Tab *active_tab = tab_manager_get_active(&g_app.tab_mgr);
                        LanguageType lang = LANG_NONE;
                        if (active_tab) {
                            lang = detect_language(active_tab->filename);
                        }
                        
                        // Tokenize the line for highlighting
                        HighlightToken tokens[32];
                        int token_count = 0;
                        highlight_line(display_text + chars_processed, segment_len, lang, tokens, &token_count);
                        
                        // Render text with syntax highlighting
                        int seg_x = x;
                        int remaining_len = segment_len;
                        int text_offset = chars_processed;
                        
                        // Sort tokens by start position and render each segment
                        // Find the first token that starts at or after scroll position
                        for (int t = 0; t < token_count && remaining_len > 0; t++) {
                            HighlightToken *tok = &tokens[t];
                            
                            // Calculate token boundaries relative to our segment
                            int tok_start = tok->start;
                            int tok_end = tok->end;
                            
                            // Skip tokens before scroll position
                            if (tok_end <= seg_start_disp - chars_processed) continue;
                            if (tok_start < seg_start_disp - chars_processed) {
                                tok_start = seg_start_disp - chars_processed;
                            }
                            
                            // Calculate render start position
                            int render_start = tok_start;
                            if (render_start < 0) render_start = 0;
                            if (render_start >= segment_len) break;
                            
                            // Render text before this token if any
                            if (render_start > (segment_len - remaining_len)) {
                                int pre_len = render_start - (segment_len - remaining_len);
                                if (pre_len > 0) {
                                    SetTextColor(mem_dc, g_app.current_theme->colors.editor_text);
                                    SetBkMode(mem_dc, TRANSPARENT);
                                    int pre_x = seg_x + (render_start - (segment_len - remaining_len)) * editor->viewport.char_width;
                                    TextOutA(mem_dc, pre_x, visual_line_y, 
                                             display_text + text_offset + (render_start - (segment_len - remaining_len)), 
                                             pre_len);
                                }
                            }
                            
                            // Render the token itself
                            int tok_len = tok_end - tok_start;
                            if (tok_len > segment_len - tok_start) tok_len = segment_len - tok_start;
                            if (tok_len > 0 && tok_start < segment_len) {
                                COLORREF color = get_token_color(tok->type);
                                SetTextColor(mem_dc, color);
                                SetBkMode(mem_dc, TRANSPARENT);
                                int tok_x = seg_x + (tok_start - (segment_len - remaining_len)) * editor->viewport.char_width;
                                // Use tok->start (original) not tok_start (modified) for text pointer
                                // tok_start is adjusted for scroll position but display_text already has text_offset
                                TextOutA(mem_dc, tok_x, visual_line_y, 
                                         display_text + text_offset + tok->start, 
                                         tok_len);
                            }
                            
                            remaining_len -= tok_len;
                        }
                        
                        // Render any remaining text without highlighting
                        if (remaining_len > 0) {
                            SetTextColor(mem_dc, g_app.current_theme->colors.editor_text);
                            SetBkMode(mem_dc, TRANSPARENT);
                            TextOutA(mem_dc, seg_x + (segment_len - remaining_len) * editor->viewport.char_width, 
                                     visual_line_y, 
                                     display_text + text_offset + (segment_len - remaining_len), 
                                     remaining_len);
                        }
                    } else {
                        // No highlighting - render in normal color
                        SetTextColor(mem_dc, g_app.current_theme->colors.editor_text);
                        SetBkMode(mem_dc, TRANSPARENT);
                        TextOutA(mem_dc, x, visual_line_y, display_text + chars_processed, segment_len);
                    }
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

// ============================================================================
// Line Numbers and Search Highlights
// ============================================================================

void render_set_show_line_numbers(Editor *editor, bool show) {
    if (editor) {
        editor->show_line_numbers = show;
    }
}

bool render_get_show_line_numbers(Editor *editor) {
    return editor ? editor->show_line_numbers : true;
}

void render_set_search_highlights(Editor *editor, TextPos *positions, int count) {
    if (!editor) return;
    
    // Free existing highlights
    if (editor->search_matches) {
        free(editor->search_matches);
        editor->search_matches = NULL;
    }
    
    editor->search_matches = positions;
    editor->search_match_count = count;
}

void render_clear_search_highlights(Editor *editor) {
    if (!editor) return;
    
    if (editor->search_matches) {
        free(editor->search_matches);
        editor->search_matches = NULL;
    }
    editor->search_match_count = 0;
}

// Bracket pair highlighting color
static COLORREF get_bracket_color(int bracket_type) {
    switch (bracket_type) {
        case 1: return RGB(255, 255, 0);    // Yellow for ()
        case 2: return RGB(0, 255, 255);    // Cyan for {}
        case 3: return RGB(255, 128, 255); // Magenta for []
        case 4: return RGB(128, 255, 128); // Light green for <>
        case 5: return RGB(255, 165, 0);   // Orange for ""
        case 6: return RGB(165, 255, 165); // Light green for ''
        default: return RGB(255, 255, 255);
    }
}

void render_draw_bracket_highlight(Editor *editor, HDC hdc) {
    if (!editor) return;
    if (editor->bracket_match < 0) return;
    
    TextPos match_pos = editor->bracket_match;
    TextPos caret_pos = editor->caret;
    
    // Draw highlight for matched bracket
    LineCol match_lc = buffer_pos_to_linecol(&editor->buffer, match_pos);
    LineCol caret_lc = buffer_pos_to_linecol(&editor->buffer, caret_pos);
    
    // Calculate visual position of matched bracket
    int match_visual_line = 0;
    for (int l = 0; l < match_lc.line; l++) {
        TextPos l_start = buffer_line_start(&editor->buffer, l);
        TextPos l_end = buffer_line_end(&editor->buffer, l);
        int l_len = l_end - l_start;
        if (g_app.word_wrap && l_len > editor->viewport.visible_cols) {
            match_visual_line += (l_len + editor->viewport.visible_cols - 1) / editor->viewport.visible_cols;
        } else {
            match_visual_line++;
        }
    }
    
    int match_display_col = byte_col_to_display_col(&editor->buffer, 
        buffer_line_start(&editor->buffer, match_lc.line), match_pos);
    
    int caret_visual_line = 0;
    for (int l = 0; l < caret_lc.line; l++) {
        TextPos l_start = buffer_line_start(&editor->buffer, l);
        TextPos l_end = buffer_line_end(&editor->buffer, l);
        int l_len = l_end - l_start;
        if (g_app.word_wrap && l_len > editor->viewport.visible_cols) {
            caret_visual_line += (l_len + editor->viewport.visible_cols - 1) / editor->viewport.visible_cols;
        } else {
            caret_visual_line++;
        }
    }
    
    int caret_display_col = byte_col_to_display_col(&editor->buffer,
        buffer_line_start(&editor->buffer, caret_lc.line), caret_pos);
    
    // Draw caret position bracket highlight
    HBRUSH bracket_brush = CreateSolidBrush(get_bracket_color(editor->bracket_type));
    HPEN bracket_pen = CreatePen(PS_SOLID, 2, get_bracket_color(editor->bracket_type));
    
    int tab_height = 0;  // Will be set in paint based on actual value
    
    // Draw matched bracket
    int match_x = RENDER_MARGIN_WIDTH + (match_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
    int match_y = tab_height + (match_visual_line - editor->viewport.scroll_y) * editor->viewport.line_height;
    
    if (match_x >= RENDER_MARGIN_WIDTH && match_x < 2000 && match_y >= 0 && match_y < 2000) {
        SelectObject(hdc, bracket_pen);
        Rectangle(hdc, match_x - 1, match_y, match_x + editor->viewport.char_width + 1, match_y + editor->viewport.line_height);
    }
    
    // Draw caret bracket highlight if different from match
    if (caret_pos != match_pos) {
        int caret_x = RENDER_MARGIN_WIDTH + (caret_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
        int caret_y = tab_height + (caret_visual_line - editor->viewport.scroll_y) * editor->viewport.line_height;
        
        if (caret_x >= RENDER_MARGIN_WIDTH && caret_x < 2000 && caret_y >= 0 && caret_y < 2000) {
            SelectObject(hdc, bracket_pen);
            Rectangle(hdc, caret_x - 1, caret_y, caret_x + editor->viewport.char_width + 1, caret_y + editor->viewport.line_height);
        }
    }
    
    DeleteObject(bracket_brush);
    DeleteObject(bracket_pen);
}

void render_draw_autocomplete_popup(Editor *editor, HDC hdc) {
    if (!editor) return;
    if (!editor->autocomplete.visible) return;
    if (editor->autocomplete.count <= 0) return;
    
    // Calculate position below caret
    LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
    int caret_display_col = get_caret_display_col(editor);
    
    int caret_visual_line = 0;
    for (int l = 0; l < lc.line; l++) {
        TextPos l_start = buffer_line_start(&editor->buffer, l);
        TextPos l_end = buffer_line_end(&editor->buffer, l);
        int l_len = l_end - l_start;
        if (g_app.word_wrap && l_len > editor->viewport.visible_cols) {
            caret_visual_line += (l_len + editor->viewport.visible_cols - 1) / editor->viewport.visible_cols;
        } else {
            caret_visual_line++;
        }
    }
    
    int tab_height = 0;
    int popup_x = RENDER_MARGIN_WIDTH + (caret_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
    int popup_y = tab_height + (caret_visual_line + 1 - editor->viewport.scroll_y) * editor->viewport.line_height;
    
    // Popup dimensions
    int item_height = editor->viewport.line_height;
    int max_width = 200;
    int popup_height = editor->autocomplete.count * item_height;
    if (popup_height > 200) popup_height = 200;
    
    // Draw background
    HBRUSH bg_brush = CreateSolidBrush(RGB(45, 45, 48));
    HPEN border_pen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
    
    RECT popup_rect = {popup_x, popup_y, popup_x + max_width, popup_y + popup_height};
    FillRect(hdc, &popup_rect, bg_brush);
    SelectObject(hdc, border_pen);
    Rectangle(hdc, popup_x, popup_y, popup_x + max_width, popup_y + popup_height);
    
    // Draw items
    HFONT old_font = (HFONT)SelectObject(hdc, editor->font);
    
    for (int i = 0; i < editor->autocomplete.count; i++) {
        int item_y = popup_y + i * item_height;
        
        // Highlight selected item
        if (i == editor->autocomplete.selected_index) {
            HBRUSH sel_brush = CreateSolidBrush(RGB(0, 122, 204));
            RECT sel_rect = {popup_x + 1, item_y, popup_x + max_width - 1, item_y + item_height};
            FillRect(hdc, &sel_rect, sel_brush);
            DeleteObject(sel_brush);
            SetTextColor(hdc, RGB(255, 255, 255));
        } else {
            SetTextColor(hdc, RGB(220, 220, 220));
        }
        
        SetBkMode(hdc, TRANSPARENT);
        TextOutA(hdc, popup_x + 5, item_y, editor->autocomplete.words[i].word, 
                 (int)strlen(editor->autocomplete.words[i].word));
    }
    
    SelectObject(hdc, old_font);
    DeleteObject(bg_brush);
    DeleteObject(border_pen);
}
