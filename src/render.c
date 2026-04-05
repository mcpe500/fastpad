#include "render.h"
#include "buffer.h"
#include <stdlib.h>

#define FONT_HEIGHT 16
#define FONT_WIDTH 8
#define TAB_SPACES 4  // Number of spaces per tab character

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

// Helper: get the display column of caret within its line
static int get_caret_display_col(Editor *editor) {
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
    // This prevents ghost characters from appearing
    HBRUSH bg_brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &client_rect, bg_brush);
    DeleteObject(bg_brush);

    // Adjust client rect for tab bar (only draw text below tab bar)
    client_rect.top = tab_bar_offset;

    // Get tab bar height for calculations
    int tab_height = tab_bar_offset;
    
    // Calculate visible line range
    int start_line = editor->viewport.scroll_y;
    int end_line = start_line + editor->viewport.visible_lines + 1;
    int total_lines = buffer_line_count(&editor->buffer);
    
    if (end_line > total_lines) {
        end_line = total_lines;
    }
    
    // Draw each visible line
    for (int line = start_line; line <= end_line; line++) {
        int y = tab_height + (line - start_line) * editor->viewport.line_height;

        // Get line text
        TextPos line_start = buffer_line_start(&editor->buffer, line);
        TextPos line_end = buffer_line_end(&editor->buffer, line);
        int line_length = line_end - line_start;

        char *line_text = buffer_get_text(&editor->buffer, line_start, line_end);
        if (!line_text) {
            continue;
        }

        // Expand tabs for display
        int display_length = line_length;
        char *display_text = expand_tabs(line_text, line_length, &display_length);
        if (!display_text) {
            display_text = line_text;
            display_length = line_length;
        }

        // Calculate selection for this line
        TextPos sel_start = editor->selection.start;
        TextPos sel_end = editor->selection.end;

        if (sel_start > sel_end) {
            TextPos temp = sel_start;
            sel_start = sel_end;
            sel_end = temp;
        }

        // Convert selection positions to display columns (accounting for tabs)
        bool has_selection = sel_start < sel_end;
        int sel_start_display_col = -1;
        int sel_end_display_col = -1;

        if (has_selection && sel_start < line_end && sel_end > line_start) {
            // Calculate display column where selection starts/ends within this line
            TextPos sel_start_in_line = (sel_start > line_start) ? sel_start : line_start;
            TextPos sel_end_in_line = (sel_end < line_end) ? sel_end : line_end;

            sel_start_display_col = byte_col_to_display_col(&editor->buffer, line_start, sel_start_in_line);
            sel_end_display_col = byte_col_to_display_col(&editor->buffer, line_start, sel_end_in_line);

            // Draw selection background using display columns
            int x1 = (sel_start_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
            int x2 = (sel_end_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;

            if (x2 > x1) {
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

        // Draw text using display_text (tabs expanded to spaces)
        int x = -editor->viewport.scroll_x * editor->viewport.char_width;

        if (display_length > 0) {
            if (has_selection && sel_start_display_col >= 0 && sel_end_display_col > sel_start_display_col) {
                // Draw 3 segments: before selection, selected, after selection

                // Segment 1: Before selection (normal black text)
                if (sel_start_display_col > 0) {
                    SetTextColor(hdc, RGB(0, 0, 0));
                    SetBkMode(hdc, TRANSPARENT);
                    TextOutA(hdc, x, y, display_text, sel_start_display_col);
                }

                // Segment 2: Selected portion (white text on blue background)
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);
                int sel_x = x + sel_start_display_col * editor->viewport.char_width;
                TextOutA(hdc, sel_x, y, display_text + sel_start_display_col, sel_end_display_col - sel_start_display_col);

                // Segment 3: After selection (normal black text)
                if (sel_end_display_col < display_length) {
                    SetTextColor(hdc, RGB(0, 0, 0));
                    SetBkMode(hdc, TRANSPARENT);
                    int after_x = x + sel_end_display_col * editor->viewport.char_width;
                    TextOutA(hdc, after_x, y, display_text + sel_end_display_col, display_length - sel_end_display_col);
                }
            } else {
                // No selection on this line - draw normally
                SetTextColor(hdc, RGB(0, 0, 0));
                SetBkMode(hdc, TRANSPARENT);
                TextOutA(hdc, x, y, display_text, display_length);
            }
        }

        free(line_text);
        if (display_text != line_text) {
            free(display_text);
        }
    }
    
    // Draw caret if window is focused
    // Use display column (tabs expanded) for correct positioning
    if (GetFocus() == editor->hwnd) {
        int caret_display_col = get_caret_display_col(editor);
        LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
        int caret_line = lc.line - editor->viewport.scroll_y;
        int caret_x = (caret_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
        int caret_y = tab_height + caret_line * editor->viewport.line_height;

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
