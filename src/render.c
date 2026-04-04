#include "render.h"
#include "buffer.h"
#include <stdlib.h>

#define FONT_HEIGHT 16
#define FONT_WIDTH 8

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

static void render_draw_line(HDC hdc, const char *text, int length, int x, int y, COLORREF color) {
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    
    // Draw text
    TextOutA(hdc, x, y, text, length);
}

void render_paint(Editor *editor, HDC hdc, const RECT *update_rect) {
    (void)update_rect;
    
    // Select font
    HFONT old_font = (HFONT)SelectObject(hdc, editor->font);
    
    // Get client area
    RECT client_rect;
    GetClientRect(editor->hwnd, &client_rect);
    
    // Fill background
    HBRUSH bg_brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &client_rect, bg_brush);
    DeleteObject(bg_brush);
    
    // Calculate visible line range
    int start_line = editor->viewport.scroll_y;
    int end_line = start_line + editor->viewport.visible_lines + 1;
    int total_lines = buffer_line_count(&editor->buffer);
    
    if (end_line > total_lines) {
        end_line = total_lines;
    }
    
    // Draw each visible line
    for (int line = start_line; line <= end_line; line++) {
        int y = (line - start_line) * editor->viewport.line_height;
        
        // Get line text
        TextPos line_start = buffer_line_start(&editor->buffer, line);
        TextPos line_end = buffer_line_end(&editor->buffer, line);
        int line_length = line_end - line_start;
        
        char *line_text = buffer_get_text(&editor->buffer, line_start, line_end);
        if (!line_text) {
            continue;
        }
        
        // Calculate selection for this line
        TextPos sel_start = editor->selection.start;
        TextPos sel_end = editor->selection.end;
        
        if (sel_start > sel_end) {
            TextPos temp = sel_start;
            sel_start = sel_end;
            sel_end = temp;
        }
        
        bool has_selection = sel_start < sel_end;
        
        // Draw selection background if present
        if (has_selection) {
            LineCol start_lc = buffer_pos_to_linecol(&editor->buffer, sel_start);
            LineCol end_lc = buffer_pos_to_linecol(&editor->buffer, sel_end);
            
            if (start_lc.line <= line && end_lc.line >= line) {
                int sel_start_col = (start_lc.line == line) ? start_lc.col : 0;
                int sel_end_col = (end_lc.line == line) ? end_lc.col : line_length;
                
                int x1 = (sel_start_col - editor->viewport.scroll_x) * editor->viewport.char_width;
                int x2 = (sel_end_col - editor->viewport.scroll_x) * editor->viewport.char_width;
                
                if (x2 > 0) {
                    RECT sel_rect = {
                        x1,
                        y,
                        x2,
                        y + editor->viewport.line_height
                    };
                    
                    HBRUSH sel_brush = CreateSolidBrush(RGB(0, 120, 215));
                    FillRect(hdc, &sel_rect, sel_brush);
                    DeleteObject(sel_brush);
                }
            }
        }
        
        // Draw text with selection color or normal color
        COLORREF text_color = has_selection ? RGB(255, 255, 255) : RGB(0, 0, 0);
        
        int x = -editor->viewport.scroll_x * editor->viewport.char_width;
        
        // Only draw if line is not empty
        if (line_length > 0) {
            render_draw_line(hdc, line_text, line_length, x, y, text_color);
        }
        
        free(line_text);
    }
    
    // Draw caret if window is focused
    if (GetFocus() == editor->hwnd) {
        LineCol caret_lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
        int caret_line = caret_lc.line - editor->viewport.scroll_y;
        int caret_x = (caret_lc.col - editor->viewport.scroll_x) * editor->viewport.char_width;
        int caret_y = caret_line * editor->viewport.line_height;
        
        if (caret_line >= 0 && caret_line <= editor->viewport.visible_lines) {
            CreateCaret(editor->hwnd, NULL, 1, editor->viewport.line_height);
            SetCaretPos(caret_x, caret_y);
            ShowCaret(editor->hwnd);
        } else {
            HideCaret(editor->hwnd);
        }
    } else {
        HideCaret(editor->hwnd);
    }
    
    // Restore old font
    SelectObject(hdc, old_font);
}

int render_y_to_line(Editor *editor, int y) {
    return editor->viewport.scroll_y + (y / editor->viewport.line_height);
}

int render_line_to_y(Editor *editor, int line) {
    return (line - editor->viewport.scroll_y) * editor->viewport.line_height;
}
