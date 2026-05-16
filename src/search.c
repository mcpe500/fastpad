#include "search.h"
#include "errors.h"
#include "buffer.h"
#include "editor.h"
#include "app.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int char_compare(char a, char b, bool match_case) {
    if (match_case) {
        return a == b;
    } else {
        return tolower((unsigned char)a) == tolower((unsigned char)b);
    }
}

TextPos search_find(GapBuffer *buffer, TextPos start_pos, const char *text, bool match_case) {
    if (!text || text[0] == '\0') {
        return -1;
    }
    
    int text_len = (int)strlen(text);
    int buffer_len = buffer_length(buffer);
    
    if (start_pos < 0 || start_pos >= buffer_len) {
        return -1;
    }
    
    // Search from start_pos to end
    for (int i = start_pos; i <= buffer_len - text_len; i++) {
        bool match = true;
        
        for (int j = 0; j < text_len; j++) {
            if (!char_compare(buffer_get_char(buffer, i + j), text[j], match_case)) {
                match = false;
                break;
            }
        }
        
        if (match) {
            return i;
        }
    }
    
    return -1;
}

TextPos search_find_next(GapBuffer *buffer, TextPos current_pos, const char *text, bool match_case) {
    if (!text || text[0] == '\0') {
        return -1;
    }
    
    // Search from current_pos+1 to end, then wrap to beginning
    TextPos pos = search_find(buffer, current_pos + 1, text, match_case);
    
    if (pos == -1) {
        // Wrap around
        pos = search_find(buffer, 0, text, match_case);
    }
    
    return pos;
}

TextPos search_find_prev(GapBuffer *buffer, TextPos current_pos, const char *text, bool match_case) {
    if (!text || text[0] == '\0') {
        return -1;
    }
    
    int text_len = (int)strlen(text);
    
    // Search backwards from current_pos-1 to beginning
    for (int i = current_pos - 1; i >= 0; i--) {
        bool match = true;
        
        for (int j = 0; j < text_len; j++) {
            if (i + j >= buffer_length(buffer) ||
                !char_compare(buffer_get_char(buffer, i + j), text[j], match_case)) {
                match = false;
                break;
            }
        }
        
        if (match) {
            return i;
        }
    }
    
    // Wrap around - search from end
    int buffer_len = buffer_length(buffer);
    for (int i = buffer_len - text_len; i >= current_pos; i--) {
        bool match = true;
        
        for (int j = 0; j < text_len; j++) {
            if (!char_compare(buffer_get_char(buffer, i + j), text[j], match_case)) {
                match = false;
                break;
            }
        }
        
        if (match) {
            return i;
        }
    }
    
    return -1;
}

// BUG #010 Fix: Use per-instance data instead of global state for thread safety
typedef struct {
    char find_text[256];
    bool match_case;
    Editor *editor;
    HWND parent;
    HWND edit_ctrl;
} FindDialogData;

static HWND g_find_dialog = NULL;  // Only used for singleton dialog detection

static LRESULT CALLBACK FindSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Get per-instance data
    FindDialogData *data = (FindDialogData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    // FIX BUG: Validate editor window handle before using it
    // This prevents crashes if tab was closed while dialog was open
    // or if editor pointer became stale after tab switch
    if (data && data->editor && !IsWindow(data->editor->hwnd)) {
        data->editor = NULL;  // Mark as invalid, prevent crash
    }
    
    switch (msg) {
        case WM_COMMAND: {
            WORD id = LOWORD(wParam);

            if (id == 101) { // Find Next button
                if (!data) break;
                
                // Get text from edit control
                GetWindowTextA(data->edit_ctrl, data->find_text, sizeof(data->find_text));

                if (data->find_text[0] != '\0' && data->editor) {
                    TextPos pos = search_find_next(
                        &data->editor->buffer,
                        data->editor->caret,
                        data->find_text,
                        data->match_case
                    );

                    if (pos != -1) {
                        data->editor->caret = pos;
                        data->editor->selection.start = pos;
                        data->editor->selection.end = pos + (int)strlen(data->find_text);
                        editor_scroll_to_caret(data->editor);
                        InvalidateRect(data->editor->hwnd, NULL, FALSE);
                        // Return focus to parent/editor
                        SetFocus(data->parent);
                    } else {
                        MessageBoxA(hwnd, MSG_TEXT_NOT_FOUND, DLG_FIND, MB_ICONINFORMATION);
                    }
                }
            } else if (id == 104) { // Find Previous button
                if (!data) break;
                
                GetWindowTextA(data->edit_ctrl, data->find_text, sizeof(data->find_text));

                if (data->find_text[0] != '\0' && data->editor) {
                    TextPos pos = search_find_prev(
                        &data->editor->buffer,
                        data->editor->caret,
                        data->find_text,
                        data->match_case
                    );

                    if (pos != -1) {
                        data->editor->caret = pos;
                        data->editor->selection.start = pos;
                        data->editor->selection.end = pos + (int)strlen(data->find_text);
                        editor_scroll_to_caret(data->editor);
                        InvalidateRect(data->editor->hwnd, NULL, FALSE);
                        SetFocus(data->parent);
                    } else {
                        MessageBoxA(hwnd, MSG_TEXT_NOT_FOUND, DLG_FIND, MB_ICONINFORMATION);
                    }
                }
            } else if (id == 102) { // Close button
                DestroyWindow(hwnd);
            } else if (id == 103) { // Match case checkbox
                if (data) {
                    data->match_case = IsDlgButtonChecked(hwnd, 103) == BST_CHECKED;
                }
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            // Restore focus to parent window and clean up data
            {
                FindDialogData *data = (FindDialogData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
                if (data && data->parent) {
                    SetFocus(data->parent);
                }
                free(data);
                g_find_dialog = NULL;
            }
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void search_show_dialog(HWND parent_hwnd, Editor *editor) {
    // BUG #010 Fix: Use per-instance data for thread safety
    
    // If dialog already exists, bring to front and UPDATE editor/parent in existing data
    if (g_find_dialog && IsWindow(g_find_dialog)) {
        FindDialogData *data = (FindDialogData *)GetWindowLongPtr(g_find_dialog, GWLP_USERDATA);
        if (data) {
            data->editor = editor;
            data->parent = parent_hwnd;
        }
        SetForegroundWindow(g_find_dialog);
        if (data && data->edit_ctrl) {
            SetFocus(data->edit_ctrl);
        }
        return;
    }

    // Allocate per-instance data (BUG #010 fix)
    FindDialogData *data = (FindDialogData *)calloc(1, sizeof(FindDialogData));
    if (!data) return;
    
    data->editor = editor;
    data->parent = parent_hwnd;
    
    // Register a simple window class
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = FindSubclassProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "FastPadFindDialog";
    
    if (!GetClassInfoExA(NULL, "FastPadFindDialog", &wc)) {
        RegisterClassExA(&wc);
    }
    
    // Create dialog window
    g_find_dialog = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE,
        "FastPadFindDialog",
        "Find",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_MODALFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 90,
        parent_hwnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!g_find_dialog) {
        MessageBoxA(parent_hwnd, ERR_FAILED_FIND_DIALOG, DLG_ERROR, MB_ICONERROR);
        free(data);
        return;
    }

    // Store per-instance data (BUG #010 fix)
    SetWindowLongPtr(g_find_dialog, GWLP_USERDATA, (LONG_PTR)data);

    // Create Static "Find what:" label
    CreateWindowExA(0, "STATIC", "Fi&nd what:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 12, 60, 15,
        g_find_dialog, NULL, NULL, NULL);

    // Create Edit control (use per-instance text)
    data->edit_ctrl = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", data->find_text,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
        75, 8, 180, 22,
        g_find_dialog, (HMENU)100, NULL, NULL);

    // Create "Find Next" button
    CreateWindowExA(0, "BUTTON", "&Find Next",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        260, 8, 75, 23,
        g_find_dialog, (HMENU)101, NULL, NULL);

    // Create "Find Previous" button
    CreateWindowExA(0, "BUTTON", "&Find Prev",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        340, 8, 75, 23,
        g_find_dialog, (HMENU)104, NULL, NULL);

    // Create "Match case" checkbox
    CreateWindowExA(0, "BUTTON", "&Match case",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        75, 35, 100, 18,
        g_find_dialog, (HMENU)103, NULL, NULL);

    if (data->match_case) {
        SendDlgItemMessage(g_find_dialog, 103, BM_SETCHECK, BST_CHECKED, 0);
    }

    // Create Close button
    CreateWindowExA(0, "BUTTON", "Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        340, 35, 75, 23,
        g_find_dialog, (HMENU)102, NULL, NULL);

    // Center dialog on parent
    RECT parent_rect, dialog_rect;
    GetWindowRect(parent_hwnd, &parent_rect);
    GetWindowRect(g_find_dialog, &dialog_rect);

    int x = parent_rect.left + (parent_rect.right - parent_rect.left - (dialog_rect.right - dialog_rect.left)) / 2;
    int y = parent_rect.top + (parent_rect.bottom - parent_rect.top - (dialog_rect.bottom - dialog_rect.top)) / 2;

    SetWindowPos(g_find_dialog, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    ShowWindow(g_find_dialog, SW_SHOW);
    SetFocus(data->edit_ctrl);
}

// ============================================================================
// Replace functionality
// ============================================================================

typedef struct {
    char find_text[256];
    char replace_text[256];
    bool match_case;
    bool whole_word;
    Editor *editor;
    HWND parent;
    HWND find_edit;
    HWND replace_edit;
} ReplaceDialogData;

static HWND g_replace_dialog = NULL;

static LRESULT CALLBACK ReplaceSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ReplaceDialogData *data = (ReplaceDialogData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    if (data && data->editor && !IsWindow(data->editor->hwnd)) {
        data->editor = NULL;
    }
    
    switch (msg) {
        case WM_COMMAND: {
            WORD id = LOWORD(wParam);

            if (id == 101) { // Replace button
                if (!data) break;
                
                GetWindowTextA(data->find_edit, data->find_text, sizeof(data->find_text));
                GetWindowTextA(data->replace_edit, data->replace_text, sizeof(data->replace_text));

                if (data->find_text[0] != '\0' && data->editor) {
                    TextPos next_pos;
                    TextPos pos = search_replace_next(
                        &data->editor->buffer,
                        data->editor->caret,
                        data->find_text,
                        data->replace_text,
                        data->match_case,
                        &next_pos
                    );

                    if (pos != -1) {
                        data->editor->caret = next_pos;
                        data->editor->selection.start = next_pos;
                        data->editor->selection.end = next_pos + (int)strlen(data->replace_text);
                        editor_scroll_to_caret(data->editor);
                        InvalidateRect(data->editor->hwnd, NULL, FALSE);
                        SetFocus(data->parent);
                    } else {
                        MessageBoxA(hwnd, MSG_TEXT_NOT_FOUND, DLG_FIND, MB_ICONINFORMATION);
                    }
                }
            } else if (id == 102) { // Replace All button
                if (!data) break;
                
                GetWindowTextA(data->find_edit, data->find_text, sizeof(data->find_text));
                GetWindowTextA(data->replace_edit, data->replace_text, sizeof(data->replace_text));

                if (data->find_text[0] != '\0' && data->editor) {
                    int count = search_replace_all(
                        &data->editor->buffer,
                        data->find_text,
                        data->replace_text,
                        data->match_case,
                        data->whole_word,
                        false
                    );

                    if (count > 0) {
                        char msg[100];
                        snprintf(msg, sizeof(msg), "Replaced %d occurrences", count);
                        MessageBoxA(hwnd, msg, DLG_FIND, MB_ICONINFORMATION);
                        InvalidateRect(data->editor->hwnd, NULL, FALSE);
                        SetFocus(data->parent);
                    } else {
                        MessageBoxA(hwnd, MSG_TEXT_NOT_FOUND, DLG_FIND, MB_ICONINFORMATION);
                    }
                }
            } else if (id == 103) { // Find Next button
                if (!data) break;
                
                GetWindowTextA(data->find_edit, data->find_text, sizeof(data->find_text));

                if (data->find_text[0] != '\0' && data->editor) {
                    TextPos pos = search_find_next(
                        &data->editor->buffer,
                        data->editor->caret,
                        data->find_text,
                        data->match_case
                    );

                    if (pos != -1) {
                        data->editor->caret = pos;
                        data->editor->selection.start = pos;
                        data->editor->selection.end = pos + (int)strlen(data->find_text);
                        editor_scroll_to_caret(data->editor);
                        InvalidateRect(data->editor->hwnd, NULL, FALSE);
                        SetFocus(data->parent);
                    } else {
                        MessageBoxA(hwnd, MSG_TEXT_NOT_FOUND, DLG_FIND, MB_ICONINFORMATION);
                    }
                }
            } else if (id == 104) { // Close button
                DestroyWindow(hwnd);
            } else if (id == 105) { // Match case checkbox
                if (data) {
                    data->match_case = IsDlgButtonChecked(hwnd, 105) == BST_CHECKED;
                }
            } else if (id == 106) { // Whole word checkbox
                if (data) {
                    data->whole_word = IsDlgButtonChecked(hwnd, 106) == BST_CHECKED;
                }
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY: {
            ReplaceDialogData *data = (ReplaceDialogData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if (data && data->parent) {
                SetFocus(data->parent);
            }
            free(data);
            g_replace_dialog = NULL;
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void search_show_replace_dialog(HWND parent_hwnd, Editor *editor) {
    if (g_replace_dialog && IsWindow(g_replace_dialog)) {
        ReplaceDialogData *data = (ReplaceDialogData *)GetWindowLongPtr(g_replace_dialog, GWLP_USERDATA);
        if (data) {
            data->editor = editor;
            data->parent = parent_hwnd;
        }
        SetForegroundWindow(g_replace_dialog);
        return;
    }

    ReplaceDialogData *data = (ReplaceDialogData *)calloc(1, sizeof(ReplaceDialogData));
    if (!data) return;
    
    data->editor = editor;
    data->parent = parent_hwnd;
    
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = ReplaceSubclassProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "FastPadReplaceDialog";
    
    if (!GetClassInfoExA(NULL, "FastPadReplaceDialog", &wc)) {
        RegisterClassExA(&wc);
    }
    
    g_replace_dialog = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE,
        "FastPadReplaceDialog",
        "Replace",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_MODALFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 160,
        parent_hwnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!g_replace_dialog) {
        MessageBoxA(parent_hwnd, ERR_FAILED_FIND_DIALOG, DLG_ERROR, MB_ICONERROR);
        free(data);
        return;
    }

    SetWindowLongPtr(g_replace_dialog, GWLP_USERDATA, (LONG_PTR)data);

    // Find what label
    CreateWindowExA(0, "STATIC", "Fi&nd what:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 12, 60, 15,
        g_replace_dialog, NULL, NULL, NULL);

    // Find edit control
    data->find_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", data->find_text,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
        75, 8, 180, 22,
        g_replace_dialog, (HMENU)100, NULL, NULL);

    // Replace with label
    CreateWindowExA(0, "STATIC", "Re&place with:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 42, 70, 15,
        g_replace_dialog, NULL, NULL, NULL);

    // Replace edit control
    data->replace_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", data->replace_text,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
        75, 38, 180, 22,
        g_replace_dialog, (HMENU)110, NULL, NULL);

    // Find Next button
    CreateWindowExA(0, "BUTTON", "&Find Next",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        260, 8, 75, 23,
        g_replace_dialog, (HMENU)103, NULL, NULL);

    // Replace button
    CreateWindowExA(0, "BUTTON", "&Replace",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        260, 38, 75, 23,
        g_replace_dialog, (HMENU)101, NULL, NULL);

    // Replace All button
    CreateWindowExA(0, "BUTTON", "Replace &All",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        340, 38, 75, 23,
        g_replace_dialog, (HMENU)102, NULL, NULL);

    // Match case checkbox
    CreateWindowExA(0, "BUTTON", "&Match case",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        75, 65, 100, 18,
        g_replace_dialog, (HMENU)105, NULL, NULL);

    // Whole word checkbox
    CreateWindowExA(0, "BUTTON", "&Whole word",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        175, 65, 100, 18,
        g_replace_dialog, (HMENU)106, NULL, NULL);

    // Close button
    CreateWindowExA(0, "BUTTON", "Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        340, 65, 75, 23,
        g_replace_dialog, (HMENU)104, NULL, NULL);

    RECT parent_rect, dialog_rect;
    GetWindowRect(parent_hwnd, &parent_rect);
    GetWindowRect(g_replace_dialog, &dialog_rect);

    int x = parent_rect.left + (parent_rect.right - parent_rect.left - (dialog_rect.right - dialog_rect.left)) / 2;
    int y = parent_rect.top + (parent_rect.bottom - parent_rect.top - (dialog_rect.bottom - dialog_rect.top)) / 2;

    SetWindowPos(g_replace_dialog, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    ShowWindow(g_replace_dialog, SW_SHOW);
    SetFocus(data->find_edit);
}

// ============================================================================
// Enhanced search functions
// ============================================================================

TextPos search_replace_next(GapBuffer *buffer, TextPos current_pos, const char *text,
                            const char *replace_with, bool match_case, TextPos *out_next_pos) {
    if (!text || text[0] == '\0' || !replace_with) {
        return -1;
    }
    
    // Find the text
    TextPos pos = search_find(buffer, current_pos, text, match_case);
    if (pos == -1) {
        return -1;
    }
    
    int text_len = (int)strlen(text);
    int replace_len = (int)strlen(replace_with);
    
    // Delete the found text and insert replacement
    buffer_delete(buffer, pos, text_len);
    buffer_insert(buffer, pos, replace_with, replace_len);
    
    if (out_next_pos) {
        *out_next_pos = pos + replace_len;
    }
    
    return pos;
}

int search_replace_all(GapBuffer *buffer, const char *text, const char *replace_with,
                       bool match_case, bool whole_word, bool regex) {
    (void)regex;  // Regex not implemented in basic version
    
    if (!text || text[0] == '\0' || !replace_with) {
        return 0;
    }
    
    int count = 0;
    TextPos pos = 0;
    int text_len = (int)strlen(text);
    int replace_len = (int)strlen(replace_with);
    
    while ((pos = search_find(buffer, pos, text, match_case)) != -1) {
        // Check for whole word match if needed
        if (whole_word) {
            bool is_word_start = (pos == 0) || !isalnum((unsigned char)buffer_get_char(buffer, pos - 1));
            TextPos end_pos = pos + text_len;
            bool is_word_end = (end_pos >= buffer_length(buffer)) || !isalnum((unsigned char)buffer_get_char(buffer, end_pos));
            
            if (!is_word_start || !is_word_end) {
                pos++;
                continue;
            }
        }
        
        buffer_delete(buffer, pos, text_len);
        buffer_insert(buffer, pos, replace_with, replace_len);
        
        pos += replace_len;
        count++;
    }
    
    return count;
}

TextPos* search_find_all(GapBuffer *buffer, const char *text, bool match_case,
                         bool whole_word, int *out_count) {
    if (!text || text[0] == '\0' || !out_count) {
        *out_count = 0;
        return NULL;
    }
    
    // First pass: count matches
    int capacity = 100;
    TextPos *positions = (TextPos *)malloc(sizeof(TextPos) * capacity);
    if (!positions) {
        *out_count = 0;
        return NULL;
    }
    
    int count = 0;
    TextPos pos = 0;
    int text_len = (int)strlen(text);
    
    while ((pos = search_find(buffer, pos, text, match_case)) != -1) {
        // Check for whole word match if needed
        if (whole_word) {
            bool is_word_start = (pos == 0) || !isalnum((unsigned char)buffer_get_char(buffer, pos - 1));
            TextPos end_pos = pos + text_len;
            bool is_word_end = (end_pos >= buffer_length(buffer)) || !isalnum((unsigned char)buffer_get_char(buffer, end_pos));
            
            if (!is_word_start || !is_word_end) {
                pos++;
                continue;
            }
        }
        
        if (count >= capacity) {
            capacity *= 2;
            TextPos *new_positions = (TextPos *)realloc(positions, sizeof(TextPos) * capacity);
            if (!new_positions) {
                free(positions);
                *out_count = 0;
                return NULL;
            }
            positions = new_positions;
        }
        
        positions[count++] = pos;
        pos += text_len;
    }
    
    *out_count = count;
    return positions;
}

// Function to repeat last search (placeholder for search state)
void search_repeat_last(Editor *editor) {
    if (!editor) return;
    // In a full implementation, would store last search string
    // For now, just do nothing - the find dialog already handles this
    (void)editor;
}

// ============================================================================
// JSON Formatting Functions
// ============================================================================

typedef struct {
    const char *error_msg;
    int error_line;
    int pos;
} JsonParseResult;

static bool json_skip_whitespace(const char *json, int len, int *pos) {
    while (*pos < len && (json[*pos] == ' ' || json[*pos] == '\t' || json[*pos] == '\n' || json[*pos] == '\r')) {
        (*pos)++;
    }
    return true;
}

static int json_count_lines(const char *json, int pos) {
    int lines = 1;
    for (int i = 0; i < pos && i < (int)strlen(json); i++) {
        if (json[i] == '\n') lines++;
    }
    return lines;
}

static bool json_parse_value(const char *json, int len, int *pos, char **out_error, int *out_line);

static bool json_parse_string(const char *json, int len, int *pos) {
    if (*pos >= len || json[*pos] != '"') return false;
    (*pos)++;
    while (*pos < len) {
        if (json[*pos] == '\\' && *pos + 1 < len) {
            (*pos) += 2;
        } else if (json[*pos] == '"') {
            (*pos)++;
            return true;
        } else {
            (*pos)++;
        }
    }
    return false;
}

static bool json_parse_object(const char *json, int len, int *pos, char **out_error, int *out_line) {
    if (*pos >= len || json[*pos] != '{') {
        if (out_error) {
            *out_error = strdup("Expected '{'");
        }
        return false;
    }
    (*pos)++;
    json_skip_whitespace(json, len, pos);
    
    if (*pos < len && json[*pos] == '}') {
        (*pos)++;
        return true;
    }
    
    while (*pos < len) {
        json_skip_whitespace(json, len, pos);
        if (*pos >= len || json[*pos] != '"') {
            if (out_error) *out_error = strdup("Expected property name");
            return false;
        }
        
        if (!json_parse_string(json, len, pos)) {
            if (out_error) *out_error = strdup("Invalid string in object");
            return false;
        }
        
        json_skip_whitespace(json, len, pos);
        if (*pos >= len || json[*pos] != ':') {
            if (out_error) *out_error = strdup("Expected ':'");
            return false;
        }
        (*pos)++;
        
        json_skip_whitespace(json, len, pos);
        if (!json_parse_value(json, len, pos, out_error, out_line)) {
            return false;
        }
        
        json_skip_whitespace(json, len, pos);
        if (*pos < len && json[*pos] == ',') {
            (*pos)++;
        } else if (*pos < len && json[*pos] == '}') {
            (*pos)++;
            return true;
        } else if (*pos >= len) {
            if (out_error) *out_error = strdup("Unexpected end of object");
            return false;
        } else {
            if (out_error) *out_error = strdup("Expected ',' or '}'");
            return false;
        }
    }
    
    if (out_error) *out_error = strdup("Unclosed object");
    return false;
}

static bool json_parse_array(const char *json, int len, int *pos, char **out_error, int *out_line) {
    if (*pos >= len || json[*pos] != '[') {
        if (out_error) *out_error = strdup("Expected '['");
        return false;
    }
    (*pos)++;
    json_skip_whitespace(json, len, pos);
    
    if (*pos < len && json[*pos] == ']') {
        (*pos)++;
        return true;
    }
    
    while (*pos < len) {
        json_skip_whitespace(json, len, pos);
        if (!json_parse_value(json, len, pos, out_error, out_line)) {
            return false;
        }
        
        json_skip_whitespace(json, len, pos);
        if (*pos < len && json[*pos] == ',') {
            (*pos)++;
        } else if (*pos < len && json[*pos] == ']') {
            (*pos)++;
            return true;
        } else if (*pos >= len) {
            if (out_error) *out_error = strdup("Unexpected end of array");
            return false;
        } else {
            if (out_error) *out_error = strdup("Expected ',' or ']'");
            return false;
        }
    }
    
    if (out_error) *out_error = strdup("Unclosed array");
    return false;
}

static bool json_parse_value(const char *json, int len, int *pos, char **out_error, int *out_line) {
    json_skip_whitespace(json, len, pos);
    
    if (*pos >= len) {
        if (out_error) *out_error = strdup("Unexpected end of input");
        return false;
    }
    
    int error_line = json_count_lines(json, *pos);
    
    switch (json[*pos]) {
        case '"':
            if (!json_parse_string(json, len, pos)) {
                if (out_error && !*out_error) *out_error = strdup("Invalid string");
                if (out_line) *out_line = error_line;
                return false;
            }
            return true;
        case '{':
            return json_parse_object(json, len, pos, out_error, out_line);
        case '[':
            return json_parse_array(json, len, pos, out_error, out_line);
        case 't':
            if (*pos + 4 <= len && strncmp(json + *pos, "true", 4) == 0) {
                *pos += 4;
                return true;
            }
            if (out_error) *out_error = strdup("Invalid 'true'");
            if (out_line) *out_line = error_line;
            return false;
        case 'f':
            if (*pos + 5 <= len && strncmp(json + *pos, "false", 5) == 0) {
                *pos += 5;
                return true;
            }
            if (out_error) *out_error = strdup("Invalid 'false'");
            if (out_line) *out_line = error_line;
            return false;
        case 'n':
            if (*pos + 4 <= len && strncmp(json + *pos, "null", 4) == 0) {
                *pos += 4;
                return true;
            }
            if (out_error) *out_error = strdup("Invalid 'null'");
            if (out_line) *out_line = error_line;
            return false;
        default:
            // Try to parse number
            if (json[*pos] == '-' || (json[*pos] >= '0' && json[*pos] <= '9')) {
                (*pos)++;
                while (*pos < len && ((json[*pos] >= '0' && json[*pos] <= '9') || json[*pos] == '.' || json[*pos] == 'e' || json[*pos] == 'E' || json[*pos] == '+' || json[*pos] == '-')) {
                    (*pos)++;
                }
                return true;
            }
            if (out_error) *out_error = strdup("Invalid value");
            if (out_line) *out_line = error_line;
            return false;
    }
}

bool json_validate(GapBuffer *buffer, char **out_error, int *out_error_line) {
    if (!buffer || !out_error) return false;
    
    *out_error = NULL;
    if (out_error_line) *out_error_line = 0;
    
    int len = buffer_length(buffer);
    if (len == 0) {
        *out_error = strdup("Empty buffer");
        return false;
    }
    
    char *json = (char *)malloc(len + 1);
    if (!json) return false;
    
    for (int i = 0; i < len; i++) {
        json[i] = buffer_get_char(buffer, i);
    }
    json[len] = '\0';
    
    int pos = 0;
    int error_line = 0;
    bool result = json_parse_value(json, len, &pos, out_error, &error_line);
    
    if (result) {
        json_skip_whitespace(json, len, &pos);
        if (pos < len) {
            result = false;
            if (out_error) *out_error = strdup("Extra characters after JSON");
        }
    }
    
    if (!result && out_error_line) {
        *out_error_line = error_line;
    }
    
    free(json);
    return result;
}

static void json_format_value(const char *json, int len, int *pos, int indent, char **out, int *out_size, int *out_cap) {
    json_skip_whitespace(json, len, pos);
    
    if (*pos >= len) return;
    
    switch (json[*pos]) {
        case '"': {
            // String
            (*out)[*out_size] = '"';
            (*out_size)++;
            (*pos)++;
            while (*pos < len && json[*pos] != '"') {
                if (*pos + 1 < len && json[*pos] == '\\') {
                    // Escape sequence
                    if (*out_size + 2 > *out_cap) {
                        *out_cap *= 2;
                        *out = (char *)realloc(*out, *out_cap);
                    }
                    (*out)[*out_size++] = json[*pos];
                    (*out)[*out_size++] = json[++(*pos)];
                } else {
                    if (*out_size + 1 > *out_cap) {
                        *out_cap *= 2;
                        *out = (char *)realloc(*out, *out_cap);
                    }
                    (*out)[*out_size++] = json[*pos];
                }
                (*pos)++;
            }
            if (*pos < len && json[*pos] == '"') {
                (*out)[*out_size++] = '"';
                (*pos)++;
            }
            break;
        }
        case '{': {
            (*out)[*out_size++] = '{';
            (*pos)++;
            json_skip_whitespace(json, len, pos);
            
            if (*pos < len && json[*pos] == '}') {
                (*out)[*out_size++] = '}';
                (*pos)++;
                break;
            }
            
            while (*pos < len) {
                json_skip_whitespace(json, len, pos);
                if (*pos >= len) break;
                
                // Add indent
                if (*out_size + indent * 4 > *out_cap) {
                    *out_cap *= 2;
                    *out = (char *)realloc(*out, *out_cap);
                }
                for (int i = 0; i < indent; i++) {
                    (*out)[*out_size++] = ' ';
                    (*out)[*out_size++] = ' ';
                    (*out)[*out_size++] = ' ';
                    (*out)[*out_size++] = ' ';
                }
                
                // Property name
                if (json[*pos] == '"') {
                    (*out)[*out_size++] = '"';
                    (*pos)++;
                    while (*pos < len && json[*pos] != '"') {
                        if (*pos + 1 < len && json[*pos] == '\\') {
                            (*out)[*out_size++] = json[*pos];
                            (*out)[*out_size++] = json[++(*pos)];
                        } else {
                            (*out)[*out_size++] = json[*pos];
                        }
                        (*pos)++;
                    }
                    if (*pos < len) {
                        (*out)[*out_size++] = '"';
                        (*pos)++;
                    }
                }
                
                json_skip_whitespace(json, len, pos);
                if (*pos < len && json[*pos] == ':') {
                    (*out)[*out_size++] = ':';
                    (*out)[*out_size++] = ' ';
                    (*pos)++;
                }
                
                json_format_value(json, len, pos, indent + 1, out, out_size, out_cap);
                
                json_skip_whitespace(json, len, pos);
                if (*pos < len && json[*pos] == ',') {
                    (*out)[*out_size++] = ',';
                    (*pos)++;
                } else if (*pos < len && json[*pos] == '}') {
                    break;
                }
            }
            
            json_skip_whitespace(json, len, pos);
            if (*pos < len && json[*pos] == '}') {
                (*out)[*out_size++] = '}';
                (*pos)++;
            }
            break;
        }
        case '[': {
            (*out)[*out_size++] = '[';
            (*pos)++;
            json_skip_whitespace(json, len, pos);
            
            if (*pos < len && json[*pos] == ']') {
                (*out)[*out_size++] = ']';
                (*pos)++;
                break;
            }
            
            while (*pos < len) {
                json_skip_whitespace(json, len, pos);
                if (*pos >= len || json[*pos] == ']') break;
                
                // Add indent
                if (*out_size + indent * 4 > *out_cap) {
                    *out_cap *= 2;
                    *out = (char *)realloc(*out, *out_cap);
                }
                for (int i = 0; i < indent; i++) {
                    (*out)[*out_size++] = ' ';
                    (*out)[*out_size++] = ' ';
                    (*out)[*out_size++] = ' ';
                    (*out)[*out_size++] = ' ';
                }
                
                json_format_value(json, len, pos, indent + 1, out, out_size, out_cap);
                
                json_skip_whitespace(json, len, pos);
                if (*pos < len && json[*pos] == ',') {
                    (*out)[*out_size++] = ',';
                    (*pos)++;
                } else if (*pos < len && json[*pos] == ']') {
                    break;
                }
            }
            
            json_skip_whitespace(json, len, pos);
            if (*pos < len && json[*pos] == ']') {
                (*out)[*out_size++] = ']';
                (*pos)++;
            }
            break;
        }
        case 't':
            if (*pos + 4 <= len && strncmp(json + *pos, "true", 4) == 0) {
                if (*out_size + 4 > *out_cap) {
                    *out_cap *= 2;
                    *out = (char *)realloc(*out, *out_cap);
                }
                strcpy(*out + *out_size, "true");
                *out_size += 4;
                *pos += 4;
            }
            break;
        case 'f':
            if (*pos + 5 <= len && strncmp(json + *pos, "false", 5) == 0) {
                if (*out_size + 5 > *out_cap) {
                    *out_cap *= 2;
                    *out = (char *)realloc(*out, *out_cap);
                }
                strcpy(*out + *out_size, "false");
                *out_size += 5;
                *pos += 5;
            }
            break;
        case 'n':
            if (*pos + 4 <= len && strncmp(json + *pos, "null", 4) == 0) {
                if (*out_size + 4 > *out_cap) {
                    *out_cap *= 2;
                    *out = (char *)realloc(*out, *out_cap);
                }
                strcpy(*out + *out_size, "null");
                *out_size += 4;
                *pos += 4;
            }
            break;
        default:
            // Number or other
            if (*out_size + 1 > *out_cap) {
                *out_cap *= 2;
                *out = (char *)realloc(*out, *out_cap);
            }
            if (json[*pos] == '-' || (json[*pos] >= '0' && json[*pos] <= '9')) {
                while (*pos < len && ((json[*pos] >= '0' && json[*pos] <= '9') || json[*pos] == '.' || json[*pos] == '-' || json[*pos] == '+' || json[*pos] == 'e' || json[*pos] == 'E')) {
                    (*out)[*out_size++] = json[*pos++];
                }
            }
            break;
    }
}

bool json_format(GapBuffer *buffer, char **out_result, char **out_error) {
    if (!buffer || !out_result) return false;
    
    *out_result = NULL;
    if (out_error) *out_error = NULL;
    
    int len = buffer_length(buffer);
    if (len == 0) {
        *out_result = strdup("");
        return true;
    }
    
    // First validate
    char *error = NULL;
    int error_line = 0;
    if (!json_validate(buffer, &error, &error_line)) {
        if (out_error) *out_error = error;
        else if (error) free(error);
        return false;
    }
    
    char *json = (char *)malloc(len + 1);
    if (!json) return false;
    
    for (int i = 0; i < len; i++) {
        json[i] = buffer_get_char(buffer, i);
    }
    json[len] = '\0';
    
    int cap = len * 2 + 100;
    char *result = (char *)malloc(cap);
    if (!result) {
        free(json);
        return false;
    }
    
    int pos = 0;
    int size = 0;
    json_format_value(json, len, &pos, 0, &result, &size, &cap);
    result[size] = '\0';
    
    free(json);
    *out_result = result;
    return true;
}

bool json_minify(GapBuffer *buffer, char **out_result, char **out_error) {
    if (!buffer || !out_result) return false;
    
    *out_result = NULL;
    if (out_error) *out_error = NULL;
    
    int len = buffer_length(buffer);
    if (len == 0) {
        *out_result = strdup("");
        return true;
    }
    
    // First validate
    char *error = NULL;
    int error_line = 0;
    if (!json_validate(buffer, &error, &error_line)) {
        if (out_error) *out_error = error;
        else if (error) free(error);
        return false;
    }
    
    char *json = (char *)malloc(len + 1);
    if (!json) return false;
    
    for (int i = 0; i < len; i++) {
        json[i] = buffer_get_char(buffer, i);
    }
    json[len] = '\0';
    
    int cap = len + 1;
    char *result = (char *)malloc(cap);
    if (!result) {
        free(json);
        return false;
    }
    
    int j = 0;
    for (int i = 0; i < len; i++) {
        if (json[i] != ' ' && json[i] != '\t' && json[i] != '\n' && json[i] != '\r') {
            result[j++] = json[i];
        }
    }
    result[j] = '\0';
    
    free(json);
    *out_result = result;
    return true;
}
