#include "search.h"
#include "buffer.h"
#include "editor.h"
#include <stdlib.h>
#include <string.h>

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

// Global find dialog state
static char g_find_text[256] = {0};
static bool g_find_match_case = false;
static HWND g_find_dialog = NULL;
static HWND g_find_edit = NULL;
static Editor *g_find_editor = NULL;

// Find dialog window procedure
static HWND g_find_parent = NULL;

static LRESULT CALLBACK FindSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            
            if (id == 101) { // Find Next button
                // Get text from edit control
                GetWindowTextA(g_find_edit, g_find_text, sizeof(g_find_text));
                
                if (g_find_text[0] != '\0' && g_find_editor) {
                    TextPos pos = search_find_next(
                        &g_find_editor->buffer,
                        g_find_editor->caret,
                        g_find_text,
                        g_find_match_case
                    );
                    
                    if (pos != -1) {
                        g_find_editor->caret = pos;
                        g_find_editor->selection.start = pos;
                        g_find_editor->selection.end = pos + (int)strlen(g_find_text);
                        editor_scroll_to_caret(g_find_editor);
                        InvalidateRect(g_find_editor->hwnd, NULL, FALSE);
                        // Return focus to parent/editor, not the find dialog
                        SetFocus(g_find_parent);
                    } else {
                        MessageBoxA(hwnd, "Text not found.", "Find", MB_ICONINFORMATION);
                    }
                }
            } else if (id == 102) { // Close button
                DestroyWindow(hwnd);
            } else if (id == 103) { // Match case checkbox
                g_find_match_case = IsDlgButtonChecked(hwnd, 103) == BST_CHECKED;
            }
            break;
        }
        
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            // Restore focus to parent window
            if (g_find_parent) {
                SetFocus(g_find_parent);
            }
            g_find_dialog = NULL;
            g_find_editor = NULL;
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void search_show_dialog(HWND parent_hwnd, Editor *editor) {
    // If dialog already exists, bring to front and set editor
    if (g_find_dialog && IsWindow(g_find_dialog)) {
        g_find_editor = editor;
        SetForegroundWindow(g_find_dialog);
        SetFocus(g_find_edit);
        return;
    }
    
    g_find_editor = editor;
    g_find_parent = parent_hwnd;
    
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
        CW_USEDEFAULT, CW_USEDEFAULT, 350, 90,
        parent_hwnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!g_find_dialog) {
        MessageBoxA(parent_hwnd, "Failed to create find dialog.", "Error", MB_ICONERROR);
        return;
    }
    
    // Create Static "Find what:" label
    CreateWindowExA(0, "STATIC", "Fi&nd what:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 12, 60, 15,
        g_find_dialog, NULL, NULL, NULL);
    
    // Create Edit control
    g_find_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_find_text,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
        75, 8, 180, 22,
        g_find_dialog, (HMENU)100, NULL, NULL);
    
    // Create "Find Next" button
    CreateWindowExA(0, "BUTTON", "&Find Next",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        260, 8, 75, 23,
        g_find_dialog, (HMENU)101, NULL, NULL);
    
    // Create "Match case" checkbox
    CreateWindowExA(0, "BUTTON", "&Match case",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        75, 35, 100, 18,
        g_find_dialog, (HMENU)103, NULL, NULL);
    
    if (g_find_match_case) {
        SendDlgItemMessage(g_find_dialog, 103, BM_SETCHECK, BST_CHECKED, 0);
    }
    
    // Create Close button
    CreateWindowExA(0, "BUTTON", "Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        260, 35, 75, 23,
        g_find_dialog, (HMENU)102, NULL, NULL);
    
    // Center dialog on parent
    RECT parent_rect, dialog_rect;
    GetWindowRect(parent_hwnd, &parent_rect);
    GetWindowRect(g_find_dialog, &dialog_rect);
    
    int x = parent_rect.left + (parent_rect.right - parent_rect.left - (dialog_rect.right - dialog_rect.left)) / 2;
    int y = parent_rect.top + (parent_rect.bottom - parent_rect.top - (dialog_rect.bottom - dialog_rect.top)) / 2;
    
    SetWindowPos(g_find_dialog, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    ShowWindow(g_find_dialog, SW_SHOW);
    SetFocus(g_find_edit);
}
