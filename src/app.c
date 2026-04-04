#include "app.h"
#include "editor.h"
#include "render.h"
#include "fileio.h"
#include "buffer.h"
#include "search.h"
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

#define IDC_EDIT 1001
#define IDC_BUTTON_FIND 1002
#define IDC_CHECK_CASE 1003
#define ID_STATUSBAR 2001

// Menu item IDs
#define ID_FILE_NEW 4001
#define ID_FILE_OPEN 4002
#define ID_FILE_SAVE 4003
#define ID_FILE_SAVEAS 4004
#define ID_FILE_EXIT 4005
#define ID_EDIT_UNDO 4010
#define ID_EDIT_REDO 4011
#define ID_EDIT_CUT 4012
#define ID_EDIT_COPY 4013
#define ID_EDIT_PASTE 4014
#define ID_EDIT_SELECTALL 4015
#define ID_EDIT_FIND 4016
#define ID_EDIT_FINDNEXT 4017
#define ID_VIEW_WORDWRAP 4020
#define ID_VIEW_STATUSBAR 4021
#define ID_HELP_ABOUT 4030

App g_app;

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App *app = (App *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
            app = (App *)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)app);
            
            // Create status bar
            app->statusbar = CreateWindowA(
                "msctls_statusbar32",
                NULL,
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                0, 0, 0, 0,
                hwnd,
                (HMENU)ID_STATUSBAR,
                NULL,
                NULL
            );
            
            // Initialize editor
            editor_init(&app->editor, hwnd);
            render_init(&app->editor);
            
            // Set initial filename
            strcpy(app->filename, "Untitled");
            app->editor.modified = false;
            app->show_statusbar = true;
            app->word_wrap = false;
            
            app_update_title(app);
            break;
        }
            
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            
            app_on_size(app, width, height);
            break;
        }
            
        case WM_PAINT: {
            app_on_paint(app);
            break;
        }
            
        case WM_CHAR: {
            app_on_char(app, wParam);
            break;
        }
            
        case WM_KEYDOWN: {
            app_on_keydown(app, wParam);
            break;
        }
            
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            bool shift = (wParam & MK_SHIFT) != 0;
            app_on_lbuttondown(app, x, y, shift);
            SetCapture(hwnd);
            break;
        }
            
        case WM_MOUSEMOVE: {
            if (GetCapture() == hwnd) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                app_on_mousemove(app, x, y, true);
            }
            break;
        }
            
        case WM_LBUTTONUP: {
            if (GetCapture() == hwnd) {
                ReleaseCapture();
                app_on_lbuttonup(app);
            }
            break;
        }
            
        case WM_SETFOCUS: {
            app_on_setfocus(app);
            break;
        }
            
        case WM_KILLFOCUS: {
            HideCaret(hwnd);
            break;
        }
            
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            app_on_mousewheel(app, delta);
            break;
        }
            
        case WM_COMMAND: {
            app_on_command(app, wParam);
            break;
        }
            
        case WM_CLOSE: {
            if (app_check_unsaved(app)) {
                DestroyWindow(hwnd);
            }
            break;
        }
            
        case WM_DESTROY: {
            app_free(app);
            PostQuitMessage(0);
            break;
        }
            
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    return 0;
}

bool app_init(App *app, HINSTANCE hInstance) {
    memset(app, 0, sizeof(App));
    
    // Register window class
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "FastPadWindowClass";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class.", "Error", MB_ICONERROR);
        return false;
    }
    
    app->running = true;
    return true;
}

void app_free(App *app) {
    editor_free(&app->editor);
}

HWND app_create_window(App *app, HINSTANCE hInstance) {
    app->hwnd = CreateWindowExA(
        0,
        "FastPadWindowClass",
        "FastPad",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        NULL,
        NULL,
        hInstance,
        app
    );
    
    if (!app->hwnd) {
        MessageBoxA(NULL, "Failed to create window.", "Error", MB_ICONERROR);
        return NULL;
    }
    
    // Create menu
    app->menu = CreateMenu();
    
    // File menu
    HMENU file_menu = CreatePopupMenu();
    AppendMenuA(file_menu, MF_STRING, ID_FILE_NEW, "&New\tCtrl+N");
    AppendMenuA(file_menu, MF_STRING, ID_FILE_OPEN, "&Open...\tCtrl+O");
    AppendMenuA(file_menu, MF_STRING, ID_FILE_SAVE, "&Save\tCtrl+S");
    AppendMenuA(file_menu, MF_STRING, ID_FILE_SAVEAS, "Save &As...\tCtrl+Shift+S");
    AppendMenuA(file_menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(file_menu, MF_STRING, ID_FILE_EXIT, "E&xit");
    AppendMenuA(app->menu, MF_POPUP, (UINT_PTR)file_menu, "&File");
    
    // Edit menu
    HMENU edit_menu = CreatePopupMenu();
    AppendMenuA(edit_menu, MF_STRING, ID_EDIT_UNDO, "&Undo\tCtrl+Z");
    AppendMenuA(edit_menu, MF_STRING, ID_EDIT_REDO, "&Redo\tCtrl+Y");
    AppendMenuA(edit_menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(edit_menu, MF_STRING, ID_EDIT_CUT, "Cu&t\tCtrl+X");
    AppendMenuA(edit_menu, MF_STRING, ID_EDIT_COPY, "&Copy\tCtrl+C");
    AppendMenuA(edit_menu, MF_STRING, ID_EDIT_PASTE, "&Paste\tCtrl+V");
    AppendMenuA(edit_menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(edit_menu, MF_STRING, ID_EDIT_SELECTALL, "Select &All\tCtrl+A");
    AppendMenuA(edit_menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(edit_menu, MF_STRING, ID_EDIT_FIND, "&Find...\tCtrl+F");
    AppendMenuA(app->menu, MF_POPUP, (UINT_PTR)edit_menu, "&Edit");
    
    // View menu
    HMENU view_menu = CreatePopupMenu();
    AppendMenuA(view_menu, MF_STRING | (app->word_wrap ? MF_CHECKED : MF_UNCHECKED), 
                ID_VIEW_WORDWRAP, "&Word Wrap");
    AppendMenuA(view_menu, MF_STRING | (app->show_statusbar ? MF_CHECKED : MF_UNCHECKED), 
                ID_VIEW_STATUSBAR, "&Status Bar");
    AppendMenuA(app->menu, MF_POPUP, (UINT_PTR)view_menu, "&View");
    
    // Help menu
    HMENU help_menu = CreatePopupMenu();
    AppendMenuA(help_menu, MF_STRING, ID_HELP_ABOUT, "&About");
    AppendMenuA(app->menu, MF_POPUP, (UINT_PTR)help_menu, "&Help");
    
    SetMenu(app->hwnd, app->menu);
    
    ShowWindow(app->hwnd, SW_SHOW);
    UpdateWindow(app->hwnd);
    
    return app->hwnd;
}

void app_file_new(App *app) {
    if (app_check_unsaved(app)) {
        buffer_free(&app->editor.buffer);
        buffer_init(&app->editor.buffer, 4096);
        
        app->editor.caret = 0;
        app->editor.selection.start = 0;
        app->editor.selection.end = 0;
        app->editor.modified = false;
        
        strcpy(app->filename, "Untitled");
        app_update_title(app);
        
        InvalidateRect(app->hwnd, NULL, TRUE);
    }
}

void app_file_open(App *app) {
    if (app_check_unsaved(app)) {
        char filename[MAX_PATH] = {0};
        
        if (file_open_dialog(app->hwnd, filename, MAX_PATH, &app->editor.buffer)) {
            // Extract just the filename for title
            const char *basename = strrchr(filename, '\\');
            if (basename) {
                basename++;
            } else {
                basename = filename;
            }
            
            strncpy(app->filename, filename, MAX_PATH);
            app->editor.caret = 0;
            app->editor.selection.start = 0;
            app->editor.selection.end = 0;
            app->editor.modified = false;
            
            app_update_title(app);
            InvalidateRect(app->hwnd, NULL, TRUE);
        }
    }
}

void app_file_save(App *app) {
    if (strcmp(app->filename, "Untitled") == 0) {
        // Save as
        char filename[MAX_PATH] = {0};
        
        if (file_save_dialog(app->hwnd, filename, MAX_PATH, &app->editor.buffer)) {
            strncpy(app->filename, filename, MAX_PATH);
            app->editor.modified = false;
            app_update_title(app);
        }
    } else {
        // Save to current file
        if (file_save(app->hwnd, app->filename, &app->editor.buffer)) {
            app->editor.modified = false;
            app_update_title(app);
        }
    }
}

void app_file_save_as(App *app) {
    char filename[MAX_PATH] = {0};
    
    if (file_save_dialog(app->hwnd, filename, MAX_PATH, &app->editor.buffer)) {
        strncpy(app->filename, filename, MAX_PATH);
        app->editor.modified = false;
        app_update_title(app);
    }
}

void app_exit(App *app) {
    if (app_check_unsaved(app)) {
        DestroyWindow(app->hwnd);
    }
}

void app_update_title(App *app) {
    char title[MAX_PATH + 20];
    
    const char *basename = strrchr(app->filename, '\\');
    if (basename) {
        basename++;
    } else {
        basename = app->filename;
    }
    
    if (app->editor.modified) {
        snprintf(title, sizeof(title), "*%s - FastPad", basename);
    } else {
        snprintf(title, sizeof(title), "%s - FastPad", basename);
    }
    
    SetWindowTextA(app->hwnd, title);
}

void app_update_statusbar(App *app) {
    if (!app->statusbar || !app->show_statusbar) {
        return;
    }
    
    int line, col;
    editor_get_linecol(&app->editor, &line, &col);
    
    char text[100];
    snprintf(text, sizeof(text), "Ln %d, Col %d  %s", 
             line, col, 
             app->editor.modified ? "Modified" : "");
    
    // Status bar parts
    int parts[3];
    RECT rect;
    GetClientRect(app->hwnd, &rect);
    
    parts[0] = rect.right / 3;
    parts[1] = (rect.right * 2) / 3;
    parts[2] = -1;
    
    SendMessage(app->statusbar, SB_SETPARTS, 3, (LPARAM)parts);
    SendMessage(app->statusbar, SB_SETTEXTA, 0, (LPARAM)text);
}

bool app_check_unsaved(App *app) {
    if (app->editor.modified) {
        int result = MessageBoxA(
            app->hwnd,
            "Save changes?",
            "FastPad",
            MB_YESNOCANCEL | MB_ICONQUESTION
        );
        
        if (result == IDYES) {
            app_file_save(app);
            return !app->editor.modified; // Return true if save succeeded
        } else if (result == IDCANCEL) {
            return false;
        }
    }
    
    return true;
}

void app_on_command(App *app, WPARAM wParam) {
    switch (LOWORD(wParam)) {
        case ID_FILE_NEW:
            app_file_new(app);
            break;
            
        case ID_FILE_OPEN:
            app_file_open(app);
            break;
            
        case ID_FILE_SAVE:
            app_file_save(app);
            break;
            
        case ID_FILE_SAVEAS:
            app_file_save_as(app);
            break;
            
        case ID_FILE_EXIT:
            app_exit(app);
            break;
            
        case ID_EDIT_UNDO:
            editor_undo(&app->editor);
            break;
            
        case ID_EDIT_REDO:
            editor_redo(&app->editor);
            break;
            
        case ID_EDIT_CUT:
            editor_cut(&app->editor);
            break;
            
        case ID_EDIT_COPY:
            editor_copy(&app->editor);
            break;
            
        case ID_EDIT_PASTE:
            editor_paste(&app->editor);
            break;
            
        case ID_EDIT_SELECTALL:
            editor_select_all(&app->editor);
            break;
            
        case ID_EDIT_FIND:
            search_show_dialog(app->hwnd, &app->editor);
            break;
            
        case ID_VIEW_WORDWRAP:
            app->word_wrap = !app->word_wrap;
            CheckMenuItem(app->menu, ID_VIEW_WORDWRAP, 
                         app->word_wrap ? MF_CHECKED : MF_UNCHECKED);
            // TODO: Implement word wrap
            break;
            
        case ID_VIEW_STATUSBAR:
            app->show_statusbar = !app->show_statusbar;
            CheckMenuItem(app->menu, ID_VIEW_STATUSBAR, 
                         app->show_statusbar ? MF_CHECKED : MF_UNCHECKED);
            ShowWindow(app->statusbar, app->show_statusbar ? SW_SHOW : SW_HIDE);
            break;
            
        case ID_HELP_ABOUT:
            MessageBoxA(app->hwnd, 
                       "FastPad v1.0\nA tiny, fast, native text editor.", 
                       "About FastPad", 
                       MB_ICONINFORMATION);
            break;
    }
}

void app_on_size(App *app, int width, int height) {
    (void)width;
    (void)height;
    
    // Resize status bar
    if (app->statusbar) {
        SendMessage(app->statusbar, WM_SIZE, 0, 0);
    }
    
    // Resize editor
    editor_resize(&app->editor);
    app_update_statusbar(app);
}

void app_on_paint(App *app) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(app->hwnd, &ps);
    
    render_paint(&app->editor, hdc, &ps.rcPaint);
    app_update_statusbar(app);
    
    EndPaint(app->hwnd, &ps);
}

void app_on_char(App *app, WPARAM wParam) {
    char ch = (char)wParam;
    
    // Handle control characters
    if (ch == '\r') {
        return; // Handled by VK_RETURN
    }
    
    editor_char_input(&app->editor, ch);
}

void app_on_keydown(App *app, WPARAM wParam) {
    bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    if (ctrl) {
        switch (wParam) {
            case 'N': // Ctrl+N - New
                app_file_new(app);
                return;
            case 'O': // Ctrl+O - Open
                app_file_open(app);
                return;
            case 'S': // Ctrl+S - Save, Ctrl+Shift+S - Save As
                if (shift) {
                    app_file_save_as(app);
                } else {
                    app_file_save(app);
                }
                return;
            case 'F': // Ctrl+F - Find
                search_show_dialog(app->hwnd, &app->editor);
                return;
            case 'A': // Ctrl+A - Select All
                editor_select_all(&app->editor);
                return;
            case 'Z': // Ctrl+Z - Undo
                editor_undo(&app->editor);
                return;
            case 'Y': // Ctrl+Y - Redo
                editor_redo(&app->editor);
                return;
            case 'X': // Ctrl+X - Cut
                editor_cut(&app->editor);
                return;
            case 'C': // Ctrl+C - Copy
                editor_copy(&app->editor);
                return;
            case 'V': // Ctrl+V - Paste
                editor_paste(&app->editor);
                return;
        }
    }
    
    editor_key_down(&app->editor, (int)wParam);
}

void app_on_lbuttondown(App *app, int x, int y, bool shift) {
    editor_mouse_click(&app->editor, x, y, shift);
}

void app_on_mousemove(App *app, int x, int y, bool dragging) {
    if (dragging) {
        editor_mouse_drag(&app->editor, x, y);
    }
}

void app_on_lbuttonup(App *app) {
    (void)app;
    // Selection dragging handled by mouse move
}

void app_on_setfocus(App *app) {
    ShowCaret(app->hwnd);
}

void app_on_mousewheel(App *app, int delta) {
    int lines = delta / WHEEL_DELTA;
    app->editor.viewport.scroll_y -= lines;
    
    // Clamp scroll position
    int total_lines = buffer_line_count(&app->editor.buffer);
    if (app->editor.viewport.scroll_y < 0) {
        app->editor.viewport.scroll_y = 0;
    } else if (app->editor.viewport.scroll_y + app->editor.viewport.visible_lines > total_lines) {
        app->editor.viewport.scroll_y = total_lines - app->editor.viewport.visible_lines;
        if (app->editor.viewport.scroll_y < 0) {
            app->editor.viewport.scroll_y = 0;
        }
    }
    
    InvalidateRect(app->hwnd, NULL, FALSE);
}
