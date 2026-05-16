#include "app.h"
#include "editor.h"
#include "render.h"
#include "fileio.h"
#include "buffer.h"
#include "search.h"
#include "tab_manager.h"
#include "errors.h"
#include "log.h"
#include "theme.h"
#include "backup.h"
#include "plugin.h"
#include "settings.h"
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

// Forward declarations
static void app_update_recent_menu(App *app);
static void recent_files_save(App *app);

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
#define ID_FILE_NEW_TAB 4006
#define ID_FILE_CLOSE_TAB 4007
#define ID_FILE_RECENT 4008
#define ID_EDIT_UNDO 4010
#define ID_EDIT_REDO 4011
#define ID_EDIT_CUT 4012
#define ID_EDIT_COPY 4013
#define ID_EDIT_PASTE 4014
#define ID_EDIT_SELECTALL 4015
#define ID_EDIT_FIND 4016
#define ID_EDIT_FINDNEXT 4017
#define ID_EDIT_REPLACE 4018
#define ID_VIEW_WORDWRAP 4020
#define ID_VIEW_STATUSBAR 4021
#define ID_VIEW_LINENUMBERS 4022
#define ID_VIEW_ZOOM_IN 4023
#define ID_VIEW_ZOOM_OUT 4024
#define ID_VIEW_ZOOM_RESET 4025
#define ID_VIEW_SPLIT_HORIZ 4026
#define ID_VIEW_SPLIT_VERT 4027
#define ID_VIEW_CLOSE_SPLIT 4028
#define ID_VIEW_HIGHLIGHT 4029
#define ID_TOOLS_FORMAT_JSON 4031
#define ID_TOOLS_MINIFY_JSON 4032
#define ID_TOOLS_VALIDATE_JSON 4033
#define ID_HELP_ABOUT 4030
#define ID_SETTINGS_SHORTCUTS 4040
#define ID_FILE_ENCODING 4050
#define ID_FILE_LINEENDING 4060
#define ID_TOOLS_BACKUP 4070
#define ID_TOOLS_BACKUP_CREATE 4071
#define ID_TOOLS_BACKUP_RESTORE 4072
#define ID_TOOLS_PLUGINS 4073
#define ID_SETTINGS_EXPORT 4074
#define ID_SETTINGS_IMPORT 4075

App g_app;

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App *app = (App *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    #ifdef DEV_BUILD
    // Log every message for debugging crashes
    // We only log important ones to avoid huge files
    if (msg == WM_CREATE || msg == WM_DESTROY || msg == WM_CLOSE || msg == WM_COMMAND) {
        log_info("WndProc: msg=0x%X, wParam=0x%X", msg, wParam);
    }
    #endif
    
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
            
            // Initialize tab manager (creates first tab automatically)
            if (!tab_manager_init(&app->tab_mgr, hwnd)) {
                MessageBoxA(hwnd, ERR_FAILED_INIT_TABS, DLG_ERROR, MB_ICONERROR);
                return -1;
            }
            
            app->show_statusbar = true;
            app->word_wrap = false;
            
            tab_manager_update_window_title(&app->tab_mgr, hwnd);
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
            
        case WM_NOTIFY: {
            NMHDR *nmhdr = (NMHDR *)lParam;
            if (nmhdr->hwndFrom == app->tab_mgr.hwnd) {
                if (nmhdr->code == TCN_SELCHANGE) {
                    // Tab selection changed
                    int sel = TabCtrl_GetCurSel(app->tab_mgr.hwnd);
                    tab_manager_on_tab_click(&app->tab_mgr, sel);
                }
            }
            break;
        }
            
        case WM_COMMAND: {
            app_on_command(app, wParam);
            break;
        }
            
        case WM_CLOSE: {
            if (tab_manager_check_unsaved(&app->tab_mgr)) {
                app->shutting_down = true;
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_DESTROY: {
            // Save history for all active tabs before closing
            for (int i = 0; i < app->tab_mgr.count; i++) {
                Tab *tab = &app->tab_mgr.tabs[i];
                if (tab->active) {
                    editor_save_history(&tab->editor, tab->filename);
                }
            }
            
            // Don't close tabs one-by-one, just free resources directly
            tab_manager_free(&app->tab_mgr);
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

    // Initialize with default theme (Classic Light)
    app->current_theme = (Theme *)theme_get_by_id(THEME_CLASSIC_LIGHT);
    strncpy(app->font_settings.font_family, "Consolas", sizeof(app->font_settings.font_family) - 1);
    app->font_settings.font_family[sizeof(app->font_settings.font_family) - 1] = '\0';
    app->font_settings.font_size = 14;
    app->font_settings.line_height = 100;
    app->font_settings.bold = false;

    // Initialize recent files
    app->recent_count = 0;
    app->show_line_numbers = true;
    app->auto_save_enabled = true;
    app->highlight_enabled = true;
    app->split_mode = SPLIT_NONE;
    app->split_ratio = 0.5f;
    app->active_split = 0;

    // Initialize zoom level (default 100, range 50-200)
    app->zoom_level = 100;

    // Initialize custom shortcuts with defaults
    app->shortcut_count = 0;
    app_load_shortcuts(app);

    // Initialize auto-save directory path
    {
        char appdata[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
            snprintf(app->auto_save_dir, MAX_PATH, "%s\\FastPad\\autosave", appdata);
            CreateDirectoryA(app->auto_save_dir, NULL);
        } else {
            app->auto_save_dir[0] = '\0';
        }
    }

    // Initialize common controls for status bar and tab control
    INITCOMMONCONTROLSEX icex = {0};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_BAR_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

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
        MessageBoxA(NULL, ERR_FAILED_REGISTER_CLASS, DLG_ERROR, MB_ICONERROR);
        return false;
    }
    
    app->running = true;
    app->last_edit_time = GetTickCount();
    return true;
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
        MessageBoxA(NULL, ERR_FAILED_CREATE_WINDOW, DLG_ERROR, MB_ICONERROR);
        return NULL;
    }
    
    // Create menu
    app->menu = CreateMenu();
    
    // File menu
    HMENU file_menu = CreatePopupMenu();
    AppendMenuA(file_menu, MF_STRING, ID_FILE_NEW, "&New\tCtrl+N");
    AppendMenuA(file_menu, MF_STRING, ID_FILE_NEW_TAB, "New Ta&b\tCtrl+T");
    AppendMenuA(file_menu, MF_STRING, ID_FILE_CLOSE_TAB, "&Close Tab\tCtrl+W");
    AppendMenuA(file_menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(file_menu, MF_STRING, ID_FILE_OPEN, "&Open...\tCtrl+O");
    AppendMenuA(file_menu, MF_STRING, ID_FILE_SAVE, "&Save\tCtrl+S");
    AppendMenuA(file_menu, MF_STRING, ID_FILE_SAVEAS, "Save &As...\tCtrl+Shift+S");
    AppendMenuA(file_menu, MF_SEPARATOR, 0, NULL);
    // Recent files submenu
    HMENU recent_menu = CreatePopupMenu();
    AppendMenuA(recent_menu, MF_STRING, 1, "(Empty)");
    EnableMenuItem(recent_menu, 1, MF_GRAYED);
    AppendMenuA(file_menu, MF_POPUP, (UINT_PTR)recent_menu, "&Recent Files");
    AppendMenuA(file_menu, MF_SEPARATOR, 0, NULL);
    // Encoding submenu
    HMENU encoding_menu = CreatePopupMenu();
    AppendMenuA(encoding_menu, MF_STRING, ID_FILE_ENCODING + 0, "UTF-8");
    AppendMenuA(encoding_menu, MF_STRING, ID_FILE_ENCODING + 1, "UTF-8 BOM");
    AppendMenuA(encoding_menu, MF_STRING, ID_FILE_ENCODING + 2, "ANSI");
    AppendMenuA(encoding_menu, MF_STRING, ID_FILE_ENCODING + 3, "UTF-16 LE");
    AppendMenuA(encoding_menu, MF_STRING, ID_FILE_ENCODING + 4, "UTF-16 BE");
    AppendMenuA(file_menu, MF_POPUP, (UINT_PTR)encoding_menu, "&Encoding");
    // Line Ending submenu
    HMENU lineending_menu = CreatePopupMenu();
    AppendMenuA(lineending_menu, MF_STRING, ID_FILE_LINEENDING + 0, "CRLF (Windows)");
    AppendMenuA(lineending_menu, MF_STRING, ID_FILE_LINEENDING + 1, "LF (Unix)");
    AppendMenuA(lineending_menu, MF_STRING, ID_FILE_LINEENDING + 2, "CR (Old Mac)");
    AppendMenuA(file_menu, MF_POPUP, (UINT_PTR)lineending_menu, "&Line Ending");
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
    AppendMenuA(edit_menu, MF_STRING, ID_EDIT_FINDNEXT, "Find &Next\tF3");
    AppendMenuA(edit_menu, MF_STRING, ID_EDIT_REPLACE, "&Replace...\tCtrl+H");
    AppendMenuA(app->menu, MF_POPUP, (UINT_PTR)edit_menu, "&Edit");

    // Tools menu
    HMENU tools_menu = CreatePopupMenu();
    AppendMenuA(tools_menu, MF_STRING, ID_TOOLS_FORMAT_JSON, "Format &JSON\tCtrl+Shift+F");
    AppendMenuA(tools_menu, MF_STRING, ID_TOOLS_MINIFY_JSON, "&Minify JSON\tCtrl+Shift+M");
    AppendMenuA(tools_menu, MF_STRING, ID_TOOLS_VALIDATE_JSON, "&Validate JSON");
    AppendMenuA(tools_menu, MF_SEPARATOR, 0, NULL);
    // Backup submenu
    HMENU backup_menu = CreatePopupMenu();
    AppendMenuA(backup_menu, MF_STRING, ID_TOOLS_BACKUP_CREATE, "&Create Backup");
    AppendMenuA(backup_menu, MF_STRING, ID_TOOLS_BACKUP_RESTORE, "&Restore from Backup...");
    AppendMenuA(tools_menu, MF_POPUP, (UINT_PTR)backup_menu, "&Backup");
    AppendMenuA(tools_menu, MF_STRING, ID_TOOLS_PLUGINS, "&Plugins...");
    AppendMenuA(app->menu, MF_POPUP, (UINT_PTR)tools_menu, "&Tools");
    
    // View menu
    HMENU view_menu = CreatePopupMenu();
    AppendMenuA(view_menu, MF_STRING | (app->word_wrap ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_WORDWRAP, "&Word Wrap");
    AppendMenuA(view_menu, MF_STRING | (app->show_statusbar ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_STATUSBAR, "&Status Bar");
    AppendMenuA(view_menu, MF_STRING | (app->show_line_numbers ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LINENUMBERS, "&Line Numbers");
    AppendMenuA(view_menu, MF_STRING | (app->highlight_enabled ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_HIGHLIGHT, "Syntax &Highlight");
    AppendMenuA(view_menu, MF_SEPARATOR, 0, NULL);
    // Zoom submenu
    HMENU zoom_menu = CreatePopupMenu();
    AppendMenuA(zoom_menu, MF_STRING, ID_VIEW_ZOOM_IN, "Zoom &In\tCtrl++");
    AppendMenuA(zoom_menu, MF_STRING, ID_VIEW_ZOOM_OUT, "Zoom &Out\tCtrl+-");
    AppendMenuA(zoom_menu, MF_STRING, ID_VIEW_ZOOM_RESET, "&Reset Zoom\tCtrl+0");
    AppendMenuA(view_menu, MF_POPUP, (UINT_PTR)zoom_menu, "&Zoom");
    AppendMenuA(view_menu, MF_SEPARATOR, 0, NULL);
    // Split submenu
    HMENU split_menu = CreatePopupMenu();
    AppendMenuA(split_menu, MF_STRING, ID_VIEW_SPLIT_HORIZ, "Split &Horizontal");
    AppendMenuA(split_menu, MF_STRING, ID_VIEW_SPLIT_VERT, "Split &Vertical");
    AppendMenuA(split_menu, MF_STRING, ID_VIEW_CLOSE_SPLIT, "&Close Split\tCtrl+W");
    AppendMenuA(view_menu, MF_POPUP, (UINT_PTR)split_menu, "&Split View");
    AppendMenuA(app->menu, MF_POPUP, (UINT_PTR)view_menu, "&View");
    
    // Settings menu
    HMENU settings_menu = CreatePopupMenu();
    AppendMenuA(settings_menu, MF_STRING, ID_SETTINGS_SHORTCUTS, "&Keyboard Shortcuts...");
    AppendMenuA(settings_menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(settings_menu, MF_STRING, ID_SETTINGS_EXPORT, "&Export Settings...");
    AppendMenuA(settings_menu, MF_STRING, ID_SETTINGS_IMPORT, "&Import Settings...");
    AppendMenuA(app->menu, MF_POPUP, (UINT_PTR)settings_menu, "&Settings");
    
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
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (!tab) return;
    
    if (tab->editor.modified) {
        if (!app_check_unsaved(app)) {
            return;
        }
    }
    
    // Clear current tab's content
    buffer_free(&tab->editor.buffer);
    buffer_init(&tab->editor.buffer, 4096);
    
    tab->editor.caret = 0;
    tab->editor.selection.start = 0;
    tab->editor.selection.end = 0;
    tab->editor.modified = false;
    
    snprintf(tab->filename, sizeof(tab->filename), "Untitled");
    snprintf(tab->title, sizeof(tab->title), "Untitled");
    
    tab_manager_update_control(&app->tab_mgr);
    tab_manager_update_window_title(&app->tab_mgr, app->hwnd);
    
    InvalidateRect(tab->hwnd, NULL, TRUE);
}

void app_file_open(App *app) {
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (!tab) return;
    
    if (tab->editor.modified) {
        if (!app_check_unsaved(app)) {
            return;
        }
    }
    
    char filename[MAX_PATH] = {0};
    
    if (file_open_dialog(app->hwnd, filename, MAX_PATH, &tab->editor.buffer)) {
        // Extract just the filename for title
        const char *basename = strrchr(filename, '\\');
        if (basename) {
            basename++;
        } else {
            basename = filename;
        }
        
        tab_manager_set_tab_filename(&app->tab_mgr, app->tab_mgr.active_index, filename);
        
        tab->editor.caret = 0;
        tab->editor.selection.start = 0;
        tab->editor.selection.end = 0;
        tab->editor.modified = false;
        
        // Load persistent history for this file
        editor_load_history(&tab->editor, filename);
        
        InvalidateRect(tab->hwnd, NULL, TRUE);
    }
}

void app_file_save(App *app) {
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (!tab) return;
    
    if (strcmp(tab->filename, "Untitled") == 0) {
        // Save as
        char filename[MAX_PATH] = {0};
        
        if (file_save_dialog(app->hwnd, filename, MAX_PATH, &tab->editor.buffer)) {
            tab_manager_set_tab_filename(&app->tab_mgr, app->tab_mgr.active_index, filename);
            tab->editor.modified = false;
            tab_manager_update_window_title(&app->tab_mgr, app->hwnd);
        }
    } else {
        // Save to current file
        if (file_save(app->hwnd, tab->filename, &tab->editor.buffer)) {
            tab->editor.modified = false;
            tab_manager_update_window_title(&app->tab_mgr, app->hwnd);
        }
    }
}

void app_file_save_as(App *app) {
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (!tab) return;
    
    char filename[MAX_PATH] = {0};
    
    if (file_save_dialog(app->hwnd, filename, MAX_PATH, &tab->editor.buffer)) {
        tab_manager_set_tab_filename(&app->tab_mgr, app->tab_mgr.active_index, filename);
        tab->editor.modified = false;
        tab_manager_update_window_title(&app->tab_mgr, app->hwnd);
    }
}

void app_exit(App *app) {
    if (tab_manager_check_unsaved(&app->tab_mgr)) {
        DestroyWindow(app->hwnd);
    }
}

void app_update_statusbar(App *app) {
    tab_manager_update_statusbar(&app->tab_mgr);
}

bool app_check_unsaved(App *app) {
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (!tab) return true;
    
    if (tab->editor.modified) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Save changes to \"%s\"?", tab->title);
        
        int result = MessageBoxA(
            app->hwnd,
            msg,
            "FastPad",
            MB_YESNOCANCEL | MB_ICONQUESTION
        );
        
        if (result == IDYES) {
            app_file_save(app);
            return !tab->editor.modified;
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
            
        case ID_FILE_NEW_TAB:
            tab_manager_new_tab(&app->tab_mgr);
            break;
            
        case ID_FILE_CLOSE_TAB:
            if (app->tab_mgr.count > 1) {
                tab_manager_close_tab(&app->tab_mgr, app->tab_mgr.active_index, app->shutting_down);
            } else {
                // Last tab - just clear it
                app_file_new(app);
            }
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

        case ID_EDIT_UNDO: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) editor_undo(&tab->editor);
            break;
        }

        case ID_EDIT_REDO: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) editor_redo(&tab->editor);
            break;
        }

        case ID_EDIT_CUT: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) editor_cut(&tab->editor);
            break;
        }

        case ID_EDIT_COPY: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) editor_copy(&tab->editor);
            break;
        }

        case ID_EDIT_PASTE: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) editor_paste(&tab->editor);
            break;
        }

        case ID_EDIT_SELECTALL: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) editor_select_all(&tab->editor);
            break;
        }

        case ID_EDIT_FIND: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) search_show_dialog(app->hwnd, &tab->editor);
            break;
        }

        case ID_EDIT_FINDNEXT: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) {
                // Repeat last search if available
                extern void search_repeat_last(Editor *editor);
                search_repeat_last(&tab->editor);
            }
            break;
        }

        case ID_EDIT_REPLACE: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) search_show_replace_dialog(app->hwnd, &tab->editor);
            break;
        }
            
        case ID_VIEW_WORDWRAP: {
            app->word_wrap = !app->word_wrap;
            CheckMenuItem(app->menu, ID_VIEW_WORDWRAP,
                         app->word_wrap ? MF_CHECKED : MF_UNCHECKED);
            
            RECT rc;
            GetClientRect(app->hwnd, &rc);
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) {
                render_resize(&tab->editor, rc.right, rc.bottom);
            }
            InvalidateRect(app->hwnd, NULL, TRUE);
            break;
        }
            
        case ID_VIEW_STATUSBAR:
            app->show_statusbar = !app->show_statusbar;
            CheckMenuItem(app->menu, ID_VIEW_STATUSBAR, 
                         app->show_statusbar ? MF_CHECKED : MF_UNCHECKED);
            ShowWindow(app->statusbar, app->show_statusbar ? SW_SHOW : SW_HIDE);
            break;

        case ID_VIEW_LINENUMBERS:
            app->show_line_numbers = !app->show_line_numbers;
            CheckMenuItem(app->menu, ID_VIEW_LINENUMBERS,
                         app->show_line_numbers ? MF_CHECKED : MF_UNCHECKED);
            // Update all editors
            for (int i = 0; i < app->tab_mgr.count; i++) {
                app->tab_mgr.tabs[i].editor.show_line_numbers = app->show_line_numbers;
            }
            InvalidateRect(app->hwnd, NULL, TRUE);
            break;

        case ID_VIEW_ZOOM_IN:
            app_handle_zoom(app, 10);
            break;

        case ID_VIEW_ZOOM_OUT:
            app_handle_zoom(app, -10);
            break;

        case ID_VIEW_ZOOM_RESET:
            app_reset_zoom(app);
            break;

        case ID_VIEW_HIGHLIGHT:
            app_toggle_highlight(app);
            break;

        case ID_VIEW_SPLIT_HORIZ:
            app_split_toggle(app, SPLIT_HORIZONTAL);
            break;

        case ID_VIEW_SPLIT_VERT:
            app_split_toggle(app, SPLIT_VERTICAL);
            break;

        case ID_VIEW_CLOSE_SPLIT:
            app_split_close(app);
            break;

        case ID_TOOLS_FORMAT_JSON: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) {
                char *result = NULL;
                char *error = NULL;
                if (json_format(&tab->editor.buffer, &result, &error)) {
                    int len = buffer_length(&tab->editor.buffer);
                    if (len > 0) {
                        buffer_delete(&tab->editor.buffer, 0, len);
                    }
                    buffer_insert(&tab->editor.buffer, 0, result, strlen(result));
                    free(result);
                    InvalidateRect(tab->hwnd, NULL, TRUE);
                } else {
                    char msg[512];
                    if (error) {
                        snprintf(msg, sizeof(msg), "JSON Format Error: %s", error);
                        free(error);
                    } else {
                        snprintf(msg, sizeof(msg), "JSON Format Error: Unknown error");
                    }
                    MessageBoxA(app->hwnd, msg, "Format JSON", MB_ICONERROR);
                }
            }
            break;
        }

        case ID_TOOLS_MINIFY_JSON: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) {
                char *result = NULL;
                char *error = NULL;
                if (json_minify(&tab->editor.buffer, &result, &error)) {
                    int len = buffer_length(&tab->editor.buffer);
                    if (len > 0) {
                        buffer_delete(&tab->editor.buffer, 0, len);
                    }
                    buffer_insert(&tab->editor.buffer, 0, result, strlen(result));
                    free(result);
                    InvalidateRect(tab->hwnd, NULL, TRUE);
                } else {
                    char msg[512];
                    if (error) {
                        snprintf(msg, sizeof(msg), "JSON Minify Error: %s", error);
                        free(error);
                    } else {
                        snprintf(msg, sizeof(msg), "JSON Minify Error: Unknown error");
                    }
                    MessageBoxA(app->hwnd, msg, "Minify JSON", MB_ICONERROR);
                }
            }
            break;
        }

        case ID_TOOLS_VALIDATE_JSON: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) {
                char *error = NULL;
                int error_line = 0;
                if (json_validate(&tab->editor.buffer, &error, &error_line)) {
                    MessageBoxA(app->hwnd, "JSON is valid!", "Validate JSON", MB_ICONINFORMATION);
                } else {
                    char msg[512];
                    if (error) {
                        snprintf(msg, sizeof(msg), "JSON Validation Error at line %d:\n%s", error_line, error);
                        free(error);
                    } else {
                        snprintf(msg, sizeof(msg), "JSON Validation Error at line %d", error_line);
                    }
                    MessageBoxA(app->hwnd, msg, "Validate JSON", MB_ICONERROR);
                }
            }
            break;
        }

        case ID_SETTINGS_SHORTCUTS:
            // Show keyboard shortcuts dialog (basic display)
            MessageBoxA(app->hwnd,
                       "Keyboard Shortcuts:\n\n"
                       "Ctrl+N - New File\n"
                       "Ctrl+O - Open File\n"
                       "Ctrl+S - Save\n"
                       "Ctrl+Shift+S - Save As\n"
                       "Ctrl+T - New Tab\n"
                       "Ctrl+W - Close Tab\n"
                       "Ctrl+Z - Undo\n"
                       "Ctrl+Y - Redo\n"
                       "Ctrl+F - Find\n"
                       "Ctrl++ - Zoom In\n"
                       "Ctrl+- - Zoom Out\n"
                       "Ctrl+0 - Reset Zoom",
                       "Keyboard Shortcuts",
                       MB_ICONINFORMATION);
            break;

        case ID_TOOLS_BACKUP_CREATE: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab && strcmp(tab->filename, "Untitled") != 0) {
                app_backup_create(app, tab->filename);
                MessageBoxA(app->hwnd, "Backup created successfully.", "Backup", MB_ICONINFORMATION);
            } else {
                MessageBoxA(app->hwnd, "Please save the file first before creating a backup.", "Backup", MB_ICONWARNING);
            }
            break;
        }

        case ID_TOOLS_BACKUP_RESTORE: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab && strcmp(tab->filename, "Untitled") != 0) {
                int count = app_backup_get_count(app, tab->filename);
                if (count == 0) {
                    MessageBoxA(app->hwnd, "No backups found for this file.", "Restore Backup", MB_ICONINFORMATION);
                } else {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Found %d backup(s). Restoring will replace current content.", count);
                    if (MessageBoxA(app->hwnd, msg, "Restore Backup", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        app_backup_restore(app, tab->filename, 0);  // Restore most recent
                        InvalidateRect(tab->hwnd, NULL, TRUE);
                    }
                }
            } else {
                MessageBoxA(app->hwnd, "No file is currently open.", "Restore Backup", MB_ICONWARNING);
            }
            break;
        }

        case ID_TOOLS_PLUGINS: {
            // Show plugins dialog
            int count = 0;
            Plugin *plugins = plugin_get_all(&count);
            if (count == 0) {
                MessageBoxA(app->hwnd, "No plugins installed.\n\nPlace plugin folders in:\n%APPDATA%\\FastPad\\plugins", "Plugins", MB_ICONINFORMATION);
            } else {
                char info[1024] = "Installed Plugins:\n\n";
                for (int i = 0; i < count; i++) {
                    char line[256];
                    snprintf(line, sizeof(line), "• %s v%s - %s\n",
                             plugins[i].manifest.name,
                             plugins[i].manifest.version,
                             plugins[i].state == PLUGIN_STATE_LOADED ? "Loaded" : "Error");
                    strncat(info, line, sizeof(info) - strlen(info) - 1);
                }
                MessageBoxA(app->hwnd, info, "Plugins", MB_ICONINFORMATION);
            }
            break;
        }

        case ID_SETTINGS_EXPORT: {
            char filepath[MAX_PATH] = {0};
            // Use save dialog
            OPENFILENAMEA ofn = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = app->hwnd;
            ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
            ofn.lpstrFile = filepath;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = "Export Settings";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
            ofn.lpstrDefExt = "json";

            if (GetSaveFileNameA(&ofn)) {
                if (app_settings_export(app, filepath)) {
                    MessageBoxA(app->hwnd, "Settings exported successfully.", "Export Settings", MB_ICONINFORMATION);
                } else {
                    MessageBoxA(app->hwnd, "Failed to export settings.", "Export Settings", MB_ICONERROR);
                }
            }
            break;
        }

        case ID_SETTINGS_IMPORT: {
            char filepath[MAX_PATH] = {0};
            OPENFILENAMEA ofn = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = app->hwnd;
            ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
            ofn.lpstrFile = filepath;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = "Import Settings";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileNameA(&ofn)) {
                if (app_settings_import(app, filepath)) {
                    MessageBoxA(app->hwnd, "Settings imported successfully.\nSome changes may require restart.", "Import Settings", MB_ICONINFORMATION);
                } else {
                    MessageBoxA(app->hwnd, "Failed to import settings.", "Import Settings", MB_ICONERROR);
                }
            }
            break;
        }

        // Encoding selection (ID_FILE_ENCODING + 0-4)
        case ID_FILE_ENCODING + 0:
        case ID_FILE_ENCODING + 1:
        case ID_FILE_ENCODING + 2:
        case ID_FILE_ENCODING + 3:
        case ID_FILE_ENCODING + 4: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) {
                tab->editor.encoding = (EncodingType)(LOWORD(wParam) - ID_FILE_ENCODING);
                tab_manager_update_statusbar(&app->tab_mgr);
            }
            break;
        }

        // Line ending selection (ID_FILE_LINEENDING + 0-2)
        case ID_FILE_LINEENDING + 0:
        case ID_FILE_LINEENDING + 1:
        case ID_FILE_LINEENDING + 2: {
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) {
                tab->editor.line_ending = (LineEndingType)(LOWORD(wParam) - ID_FILE_LINEENDING);
                tab_manager_update_statusbar(&app->tab_mgr);
            }
            break;
        }
            
        case ID_HELP_ABOUT:
            MessageBoxA(app->hwnd, 
                       "FastPad v1.0\nA tiny, fast, native text editor.", 
                       "About FastPad", 
                       MB_ICONINFORMATION);
            break;
    }
}

void app_on_size(App *app, int width, int height) {
    (void)height;

    // Resize status bar
    if (app->statusbar) {
        SendMessage(app->statusbar, WM_SIZE, 0, 0);
    }

    // Resize tab bar and active editor
    tab_manager_resize(&app->tab_mgr, width);
    app_update_statusbar(app);
}

void app_on_paint(App *app) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(app->hwnd, &ps);

    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (tab) {
        // Render editor with tab bar offset handled inside render_paint()
        render_paint(&tab->editor, hdc, &ps.rcPaint, app->tab_mgr.height);
    }

    app_update_statusbar(app);

    EndPaint(app->hwnd, &ps);
}

void app_on_char(App *app, WPARAM wParam) {
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (!tab) return;
    
    char ch = (char)wParam;
    
    // FIX: Ignore control characters (Ctrl+A, Ctrl+B, etc. are handled in WM_KEYDOWN)
    // WM_CHAR with control characters would insert garbage like SOH (0x01), STX (0x03)
    if ((unsigned char)ch < 32) {
        return;
    }
    
    // Handle control characters
    if (ch == '\r') {
        return; // Handled by VK_RETURN
    }
    
    editor_char_input(&tab->editor, ch);
}

void app_on_keydown(App *app, WPARAM wParam) {
    bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    Tab *tab = tab_manager_get_active(&app->tab_mgr);

    if (ctrl) {
        switch (wParam) {
            case 'N': // Ctrl+N - New
                app_file_new(app);
                return;
            case 'T': // Ctrl+T - New Tab
                tab_manager_new_tab(&app->tab_mgr);
                return;
            case 'W': // Ctrl+W - Close Tab or Cycle Split
                if (app->split_mode != SPLIT_NONE) {
                    app_cycle_split_focus(app);
                } else if (app->tab_mgr.count > 1) {
                    tab_manager_close_tab(&app->tab_mgr, app->tab_mgr.active_index, app->shutting_down);
                } else {
                    app_file_new(app);
                }
                return;
            case VK_TAB: // Ctrl+Tab - Next Tab
                if (shift) {
                    // Ctrl+Shift+Tab - Previous Tab
                    int prev = app->tab_mgr.active_index - 1;
                    if (prev < 0) prev = app->tab_mgr.count - 1;
                    tab_manager_switch_tab(&app->tab_mgr, prev);
                } else {
                    // Ctrl+Tab - Next Tab
                    int next = (app->tab_mgr.active_index + 1) % app->tab_mgr.count;
                    tab_manager_switch_tab(&app->tab_mgr, next);
                }
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
                if (tab) search_show_dialog(app->hwnd, &tab->editor);
                return;
            case 'A': // Ctrl+A - Select All
                if (tab) editor_select_all(&tab->editor);
                return;
            case 'Z': // Ctrl+Z - Undo
                if (tab) editor_undo(&tab->editor);
                return;
            case 'Y': // Ctrl+Y - Redo
                if (tab) editor_redo(&tab->editor);
                return;
            case 'X': // Ctrl+X - Cut
                if (tab) editor_cut(&tab->editor);
                return;
            case 'C': // Ctrl+C - Copy
                if (tab) editor_copy(&tab->editor);
                return;
            case 'V': // Ctrl+V - Paste
                if (tab) editor_paste(&tab->editor);
                return;
            case VK_OEM_PLUS: // Ctrl+= (Zoom In)
                app_handle_zoom(app, 10);
                return;
            case VK_OEM_MINUS: // Ctrl+- (Zoom Out)
                app_handle_zoom(app, -10);
                return;
            case '0': // Ctrl+0 (Reset Zoom)
                app_reset_zoom(app);
                return;
        }
    }
    
    if (tab) {
        editor_key_down(&tab->editor, (int)wParam);
    }
}

void app_on_lbuttondown(App *app, int x, int y, bool shift) {
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (!tab) return;
    
    // Check if click is in editor area (below tab bar)
    if (y < app->tab_mgr.height) return;
    
    // Adjust y for tab bar
    y -= app->tab_mgr.height;
    
    editor_mouse_click(&tab->editor, x, y, shift);
}

void app_on_mousemove(App *app, int x, int y, bool dragging) {
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (!tab) return;
    
    // Check if mouse is in editor area (below tab bar)
    if (y < app->tab_mgr.height) return;
    
    // Adjust y for tab bar
    y -= app->tab_mgr.height;
    
    if (dragging) {
        editor_mouse_drag(&tab->editor, x, y);
    }
}

void app_on_lbuttonup(App *app) {
    (void)app;
    // Selection dragging handled by mouse move
}

void app_on_setfocus(App *app) {
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (tab) {
        // FIX: When focus returns to editor, ensure caret is visible
        // Previously the caret position wasn't refreshed, causing it to
        // appear at wrong position or not show at all
        editor_scroll_to_caret(&tab->editor);
        ShowCaret(tab->hwnd);
        InvalidateRect(tab->hwnd, NULL, FALSE);
    }
}

void app_on_mousewheel(App *app, int delta) {
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (!tab) return;
    
    int lines = delta / WHEEL_DELTA;
    tab->editor.viewport.scroll_y -= lines;
    
    // Clamp scroll position
    int total_lines = buffer_line_count(&tab->editor.buffer);
    if (tab->editor.viewport.scroll_y < 0) {
        tab->editor.viewport.scroll_y = 0;
    } else if (tab->editor.viewport.scroll_y + tab->editor.viewport.visible_lines > total_lines) {
        tab->editor.viewport.scroll_y = total_lines - tab->editor.viewport.visible_lines;
        if (tab->editor.viewport.scroll_y < 0) {
            tab->editor.viewport.scroll_y = 0;
        }
    }
    
    InvalidateRect(tab->hwnd, NULL, FALSE);
}

void app_apply_theme(const Theme *theme) {
    if (!theme) return;
    
    g_app.current_theme = (Theme *)theme;
    g_app.font_settings = theme->font;
    
    // Reload font for all tabs
    for (int i = 0; i < g_app.tab_mgr.count; i++) {
        Tab *tab = &g_app.tab_mgr.tabs[i];
        // Recreate font
        HFONT old_font = tab->editor.font;
        tab->editor.font = CreateFontA(
            g_app.font_settings.font_size, 
            8, // FONT_WIDTH
            0, 0, 
            g_app.font_settings.bold ? FW_BOLD : FW_NORMAL, 
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, 
            g_app.font_settings.font_family
        );
        if (!tab->editor.font) {
            tab->editor.font = GetStockObject(SYSTEM_FIXED_FONT);
        }
        if (old_font) DeleteObject(old_font);
    }
    
    // Trigger redraw
    InvalidateRect(g_app.hwnd, NULL, TRUE);
}

// ============================================================================
// Auto Save & Session Restore
// ============================================================================

static char* app_get_session_path(void) {
    static char path[MAX_PATH];
    char appdata[MAX_PATH];
    
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) != S_OK) {
        return NULL;
    }
    
    snprintf(path, MAX_PATH, "%s\\FastPad\\session.json", appdata);
    
    // Ensure directory exists
    char dir[MAX_PATH];
    strncpy(dir, path, MAX_PATH);
    char *p = strrchr(dir, '\\');
    if (p) {
        *p = '\0';
        CreateDirectoryA(dir, NULL);
    }
    
    return path;
}

void app_init_session(App *app) {
    // Restore previous session
    app_restore_session(app);
    
    // Load recent files
    char recent_path[MAX_PATH];
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        snprintf(recent_path, MAX_PATH, "%s\\FastPad\\recent.json", appdata);
        
        FILE *f = fopen(recent_path, "r");
        if (f) {
            char line[MAX_PATH * 2];
            while (fgets(line, sizeof(line), f) && app->recent_count < MAX_RECENT_FILES) {
                // Remove trailing newline
                size_t len = strlen(line);
                if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
                if (strlen(line) > 0) {
                    strncpy(app->recent_files[app->recent_count], line, MAX_PATH - 1);
                    app->recent_files[app->recent_count][MAX_PATH - 1] = '\0';
                    app->recent_count++;
                }
            }
            fclose(f);
        }
    }
    
    // Update recent files menu
    app_update_recent_menu(app);
}

void app_save_session(App *app) {
    char *session_path = app_get_session_path();
    if (!session_path) return;
    
    FILE *f = fopen(session_path, "w");
    if (!f) return;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"tabs\": [\n");
    
    for (int i = 0; i < app->tab_mgr.count; i++) {
        Tab *tab = &app->tab_mgr.tabs[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"filename\": \"%s\",\n", tab->filename);
        fprintf(f, "      \"cursor\": %lld,\n", (long long)tab->editor.caret);
        fprintf(f, "      \"scroll_y\": %d,\n", tab->editor.viewport.scroll_y);
        fprintf(f, "      \"scroll_x\": %d,\n", tab->editor.viewport.scroll_x);
        fprintf(f, "      \"modified\": %s\n", tab->editor.modified ? "true" : "false");
        fprintf(f, "    }%s\n", (i < app->tab_mgr.count - 1) ? "," : "");
    }
    
    fprintf(f, "  ],\n");
    fprintf(f, "  \"active_index\": %d\n", app->tab_mgr.active_index);
    fprintf(f, "}\n");
    
    fclose(f);
}

void app_restore_session(App *app) {
    char *session_path = app_get_session_path();
    if (!session_path) return;
    
    FILE *f = fopen(session_path, "r");
    if (!f) return;
    
    // Parse simple JSON (no external dependencies)
    char buffer[8192];
    int bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    if (bytes_read <= 0) return;
    buffer[bytes_read] = '\0';
    
    // Clear current tabs
    while (app->tab_mgr.count > 1) {
        tab_manager_close_tab(&app->tab_mgr, 0, false);
    }
    
    // Simple parsing - look for "filename" entries
    int tab_count = 0;
    char filenames[20][MAX_PATH];
    int cursors[20] = {0};
    int scroll_ys[20] = {0};
    int scroll_xs[20] = {0};
    int actives[20] = {0};
    bool modifieds[20] = {0};
    
    // Count tabs and extract data
    char *json = buffer;
    char *next;
    
    while ((next = strstr(json, "\"filename\":")) != NULL) {
        if (tab_count >= 20) break;
        
        // Extract filename
        char *start = next + strlen("\"filename\":");
        while (*start == ' ' || *start == '\"') start++;
        char *end = start;
        while (*end && *end != '"' && *end != '\n') end++;
        
        int len = (int)(end - start);
        if (len > MAX_PATH - 1) len = MAX_PATH - 1;
        strncpy(filenames[tab_count], start, len);
        filenames[tab_count][len] = '\0';
        
        // Look for "cursor": after this filename entry
        char *cursor_pos = next + 100;
        if ((cursor_pos = strstr(cursor_pos, "\"cursor\":")) != NULL) {
            sscanf(cursor_pos + 9, "%d", &cursors[tab_count]);
        }
        if ((cursor_pos = strstr(cursor_pos, "\"scroll_y\":")) != NULL) {
            sscanf(cursor_pos + 11, "%d", &scroll_ys[tab_count]);
        }
        if ((cursor_pos = strstr(cursor_pos, "\"scroll_x\":")) != NULL) {
            sscanf(cursor_pos + 11, "%d", &scroll_xs[tab_count]);
        }
        if ((cursor_pos = strstr(cursor_pos, "\"modified\":")) != NULL) {
            modifieds[tab_count] = (strstr(cursor_pos, "true") != NULL);
        }
        
        tab_count++;
        json = end;
    }
    
    // Get active index
    if ((next = strstr(buffer, "\"active_index\":")) != NULL) {
        sscanf(next + 14, "%d", &actives[0]);
    }
    
    // Create tabs for restored files
    for (int i = 0; i < tab_count; i++) {
        if (i > 0) {
            tab_manager_new_tab(&app->tab_mgr);
        }
        
        Tab *tab = tab_manager_get_active(&app->tab_mgr);
        if (!tab) break;
        
        // Load file if not "Untitled"
        if (strcmp(filenames[i], "Untitled") != 0) {
            file_load(app->hwnd, filenames[i], &tab->editor.buffer);
            tab_manager_set_tab_filename(&app->tab_mgr, i, filenames[i]);
            
            // Check for auto-save recovery
            char auto_path[MAX_PATH];
            file_get_auto_save_path(app, filenames[i], auto_path, MAX_PATH);
            
            // If auto-save is newer, offer to recover
            FILE *af = fopen(auto_path, "r");
            if (af) {
                fclose(af);
                // For now, just use the saved file content
                // Could add dialog to ask user
            }
        }
        
        tab->editor.caret = cursors[i];
        tab->editor.viewport.scroll_y = scroll_ys[i];
        tab->editor.viewport.scroll_x = scroll_xs[i];
        
        // Update title with unsaved indicator if needed
        tab_update_title(tab);
    }
    
    // Switch to active tab
    if (actives[0] >= 0 && actives[0] < app->tab_mgr.count) {
        tab_manager_switch_tab(&app->tab_mgr, actives[0]);
    }
}

void app_on_idle(App *app) {
    if (!app->auto_save_enabled) return;
    
    // Check if 3 seconds have passed since last edit
    DWORD now = GetTickCount();
    if (now - app->last_edit_time > 3000) {
        Tab *tab = tab_manager_get_active(&app->tab_mgr);
        if (tab && tab->editor.modified && strcmp(tab->filename, "Untitled") != 0) {
            // Auto-save
            file_auto_save(app, tab->filename, &tab->editor.buffer);
            
            // Update status to show auto-saved
            app_update_statusbar(app);
        }
        app->last_edit_time = now;  // Reset to prevent repeated saves
    }
}

void app_mark_modified(App *app) {
    app->last_edit_time = GetTickCount();
}

void app_update_recent_menu(App *app) {
    // Find the Recent Files submenu by iterating through File menu items
    HMENU file_menu = GetSubMenu(app->menu, 0);
    if (!file_menu) return;
    
    int recent_menu_idx = -1;
    for (int i = 0; i < GetMenuItemCount(file_menu); i++) {
        char item_text[64];
        GetMenuStringA(file_menu, i, item_text, sizeof(item_text), MF_BYPOSITION);
        
        if (strncmp(item_text, "&Recent", 7) == 0) {
            recent_menu_idx = i;
            break;
        }
    }
    
    if (recent_menu_idx == -1) return;
    
    HMENU recent_menu = GetSubMenu(file_menu, recent_menu_idx);
    if (!recent_menu) return;
    
    // Clear existing items
    while (RemoveMenu(recent_menu, 0, MF_BYPOSITION)) {}
    
    if (app->recent_count == 0) {
        AppendMenuA(recent_menu, MF_STRING, 1, "(Empty)");
        EnableMenuItem(recent_menu, 1, MF_GRAYED);
    } else {
        for (int j = 0; j < app->recent_count; j++) {
            char display_name[MAX_PATH];
            const char *filename = app->recent_files[j];
            const char *basename = strrchr(filename, '\\');
            if (basename) basename++;
            else basename = filename;
            
            snprintf(display_name, sizeof(display_name), "%d. %s", j + 1, basename);
            AppendMenuA(recent_menu, MF_STRING, ID_FILE_RECENT + j, display_name);
        }
        
        AppendMenuA(recent_menu, MF_SEPARATOR, 0, NULL);
        AppendMenuA(recent_menu, MF_STRING, ID_FILE_RECENT + 100, "Clear Recent");
    }
}

// ============================================================================
// Recent Files Management
// ============================================================================

void recent_files_add(App *app, const char *filename) {
    if (!filename || filename[0] == '\0') return;
    if (strcmp(filename, "Untitled") == 0) return;
    
    // Check if already in list
    for (int i = 0; i < app->recent_count; i++) {
        if (strcmp(app->recent_files[i], filename) == 0) {
            // Move to front
            char temp[MAX_PATH];
            strncpy(temp, filename, MAX_PATH);
            
            // Shift others down
            for (int j = i; j > 0 && j < MAX_RECENT_FILES - 1; j--) {
                strncpy(app->recent_files[j], app->recent_files[j-1], MAX_PATH);
            }
            strncpy(app->recent_files[0], temp, MAX_PATH);
            app_update_recent_menu(app);
            recent_files_save(app);
            return;
        }
    }
    
    // Add to front
    for (int i = app->recent_count - 1; i >= 0; i--) {
        strncpy(app->recent_files[i + 1], app->recent_files[i], MAX_PATH);
        if (i == MAX_RECENT_FILES - 2) break;
    }
    
    strncpy(app->recent_files[0], filename, MAX_PATH - 1);
    app->recent_files[0][MAX_PATH - 1] = '\0';
    
    if (app->recent_count < MAX_RECENT_FILES) {
        app->recent_count++;
    }
    
    app_update_recent_menu(app);
    recent_files_save(app);
}

void recent_files_remove(App *app, const char *filename) {
    int idx = -1;
    for (int i = 0; i < app->recent_count; i++) {
        if (strcmp(app->recent_files[i], filename) == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return;
    
    // Shift remaining items
    for (int i = idx; i < app->recent_count - 1; i++) {
        strncpy(app->recent_files[i], app->recent_files[i + 1], MAX_PATH);
    }
    app->recent_count--;
    
    app_update_recent_menu(app);
    recent_files_save(app);
}

void recent_files_clear(App *app) {
    app->recent_count = 0;
    app_update_recent_menu(app);
    recent_files_save(app);
}

const char** app_get_recent_files(App *app, int *count) {
    *count = app->recent_count;
    return (const char**)app->recent_files;
}

static void recent_files_save(App *app) {
    char recent_path[MAX_PATH];
    char appdata[MAX_PATH];
    
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) != S_OK) {
        return;
    }
    
    snprintf(recent_path, MAX_PATH, "%s\\FastPad\\recent.json", appdata);
    
    // Ensure directory exists
    char dir[MAX_PATH];
    strncpy(dir, recent_path, MAX_PATH);
    char *p = strrchr(dir, '\\');
    if (p) {
        *p = '\0';
        CreateDirectoryA(dir, NULL);
    }
    
    FILE *f = fopen(recent_path, "w");
    if (!f) return;
    
    for (int i = 0; i < app->recent_count; i++) {
        fprintf(f, "%s\n", app->recent_files[i]);
    }
    
    fclose(f);
}

// ============================================================================
// Zoom Functions
// ============================================================================

void app_handle_zoom(App *app, int delta) {
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (!tab) return;
    
    // Adjust zoom level within bounds
    app->zoom_level += delta;
    if (app->zoom_level < 50) app->zoom_level = 50;
    if (app->zoom_level > 200) app->zoom_level = 200;
    
    // Calculate zoomed font size
    int base_size = app->font_settings.font_size;
    int zoomed_size = (base_size * app->zoom_level) / 100;
    if (zoomed_size < 5) zoomed_size = 5;
    if (zoomed_size > 200) zoomed_size = 200;
    
    // Recreate font for active editor with new size
    HFONT old_font = tab->editor.font;
    tab->editor.font = CreateFontA(
        zoomed_size,
        8, // FONT_WIDTH
        0, 0,
        app->font_settings.bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE,
        app->font_settings.font_family
    );
    if (!tab->editor.font) {
        tab->editor.font = GetStockObject(SYSTEM_FIXED_FONT);
    }
    if (old_font) DeleteObject(old_font);
    
    // Update viewport metrics
    HDC hdc = GetDC(tab->hwnd);
    if (hdc) {
        render_calc_metrics(&tab->editor, hdc);
        ReleaseDC(tab->hwnd, hdc);
    }
    
    // Resize editor to recalculate visible lines/cols
    RECT rc;
    GetClientRect(app->hwnd, &rc);
    int tab_top = app->tab_mgr.height;
    int editor_height = rc.bottom - tab_top;
    
    if (app->statusbar && app->show_statusbar) {
        RECT sb_rect;
        GetWindowRect(app->statusbar, &sb_rect);
        editor_height -= (sb_rect.bottom - sb_rect.top);
    }
    
    render_resize(&tab->editor, rc.right, editor_height);
    
    // Redraw
    InvalidateRect(app->hwnd, NULL, TRUE);
    app_update_statusbar(app);
}

void app_reset_zoom(App *app) {
    app->zoom_level = 100;
    app_handle_zoom(app, 0);  // Re-apply with default zoom
}

// ============================================================================
// Custom Shortcuts Functions
// ============================================================================

static void app_init_default_shortcuts(App *app) {
    // Initialize with default shortcuts
    app->shortcut_count = 0;
    
    // File operations
    app->shortcuts[app->shortcut_count].key = 'N';
    app->shortcuts[app->shortcut_count].modifiers = MOD_CONTROL;
    strncpy(app->shortcuts[app->shortcut_count].action, "file_new", 32);
    app->shortcut_count++;
    
    app->shortcuts[app->shortcut_count].key = 'O';
    app->shortcuts[app->shortcut_count].modifiers = MOD_CONTROL;
    strncpy(app->shortcuts[app->shortcut_count].action, "file_open", 32);
    app->shortcut_count++;
    
    app->shortcuts[app->shortcut_count].key = 'S';
    app->shortcuts[app->shortcut_count].modifiers = MOD_CONTROL;
    strncpy(app->shortcuts[app->shortcut_count].action, "file_save", 32);
    app->shortcut_count++;
    
    app->shortcuts[app->shortcut_count].key = 'S';
    app->shortcuts[app->shortcut_count].modifiers = MOD_CONTROL | MOD_SHIFT;
    strncpy(app->shortcuts[app->shortcut_count].action, "file_save_as", 32);
    app->shortcut_count++;
    
    // Edit operations
    app->shortcuts[app->shortcut_count].key = 'Z';
    app->shortcuts[app->shortcut_count].modifiers = MOD_CONTROL;
    strncpy(app->shortcuts[app->shortcut_count].action, "edit_undo", 32);
    app->shortcut_count++;
    
    app->shortcuts[app->shortcut_count].key = 'Y';
    app->shortcuts[app->shortcut_count].modifiers = MOD_CONTROL;
    strncpy(app->shortcuts[app->shortcut_count].action, "edit_redo", 32);
    app->shortcut_count++;
    
    // Zoom operations
    app->shortcuts[app->shortcut_count].key = VK_OEM_PLUS;
    app->shortcuts[app->shortcut_count].modifiers = MOD_CONTROL;
    strncpy(app->shortcuts[app->shortcut_count].action, "zoom_in", 32);
    app->shortcut_count++;
    
    app->shortcuts[app->shortcut_count].key = VK_OEM_MINUS;
    app->shortcuts[app->shortcut_count].modifiers = MOD_CONTROL;
    strncpy(app->shortcuts[app->shortcut_count].action, "zoom_out", 32);
    app->shortcut_count++;
    
    app->shortcuts[app->shortcut_count].key = '0';
    app->shortcuts[app->shortcut_count].modifiers = MOD_CONTROL;
    strncpy(app->shortcuts[app->shortcut_count].action, "zoom_reset", 32);
    app->shortcut_count++;
}

void app_load_shortcuts(App *app) {
    // Initialize with defaults first
    app_init_default_shortcuts(app);
    
    // Try to load from JSON file
    char shortcuts_path[MAX_PATH];
    char appdata[MAX_PATH];
    
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) != S_OK) {
        return;
    }
    
    snprintf(shortcuts_path, MAX_PATH, "%s\\FastPad\\shortcuts.json", appdata);
    
    // Ensure directory exists
    char dir[MAX_PATH];
    strncpy(dir, shortcuts_path, MAX_PATH);
    char *p = strrchr(dir, '\\');
    if (p) {
        *p = '\0';
        CreateDirectoryA(dir, NULL);
    }
    
    FILE *f = fopen(shortcuts_path, "r");
    if (!f) return;
    
    // Simple JSON parsing - just count shortcuts for now
    // In a full implementation, we'd parse the JSON properly
    char buffer[8192];
    int bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    if (bytes_read <= 0) return;
    buffer[bytes_read] = '\0';
    
    // For now, keep defaults - full JSON parsing would require more code
    // This gives us a working foundation
}

void app_save_shortcuts(App *app) {
    char shortcuts_path[MAX_PATH];
    char appdata[MAX_PATH];
    
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) != S_OK) {
        return;
    }
    
    snprintf(shortcuts_path, MAX_PATH, "%s\\FastPad\\shortcuts.json", appdata);
    
    // Ensure directory exists
    char dir[MAX_PATH];
    strncpy(dir, shortcuts_path, MAX_PATH);
    char *p = strrchr(dir, '\\');
    if (p) {
        *p = '\0';
        CreateDirectoryA(dir, NULL);
    }
    
    FILE *f = fopen(shortcuts_path, "w");
    if (!f) return;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"shortcuts\": [\n");
    for (int i = 0; i < app->shortcut_count; i++) {
        fprintf(f, "    {\n");
        fprintf(f, "      \"key\": %d,\n", app->shortcuts[i].key);
        fprintf(f, "      \"modifiers\": %d,\n", app->shortcuts[i].modifiers);
        fprintf(f, "      \"action\": \"%s\"\n", app->shortcuts[i].action);
        fprintf(f, "    }%s\n", (i < app->shortcut_count - 1) ? "," : "");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    
    fclose(f);
}

// ============================================================================
// Split View Functions
// ============================================================================

void app_split_toggle(App *app, SplitMode mode) {
    if (app->split_mode == mode) {
        // Already in this mode, close it
        app_split_close(app);
    } else {
        app->split_mode = mode;
        app->split_ratio = 0.5f;
        app->active_split = 0;
        InvalidateRect(app->hwnd, NULL, TRUE);
    }
    // Update menu checkmarks
    CheckMenuItem(app->menu, ID_VIEW_SPLIT_HORIZ,
                  app->split_mode == SPLIT_HORIZONTAL ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(app->menu, ID_VIEW_SPLIT_VERT,
                  app->split_mode == SPLIT_VERTICAL ? MF_CHECKED : MF_UNCHECKED);
}

void app_split_close(App *app) {
    app->split_mode = SPLIT_NONE;
    app->active_split = 0;
    CheckMenuItem(app->menu, ID_VIEW_SPLIT_HORIZ, MF_UNCHECKED);
    CheckMenuItem(app->menu, ID_VIEW_SPLIT_VERT, MF_UNCHECKED);
    InvalidateRect(app->hwnd, NULL, TRUE);
}

void app_cycle_split_focus(App *app) {
    if (app->split_mode == SPLIT_NONE) return;
    app->active_split = (app->active_split + 1) % 2;
    InvalidateRect(app->hwnd, NULL, TRUE);
}

// ============================================================================
// Syntax Highlighting Functions
// ============================================================================

void app_toggle_highlight(App *app) {
    app->highlight_enabled = !app->highlight_enabled;
    CheckMenuItem(app->menu, ID_VIEW_HIGHLIGHT,
                  app->highlight_enabled ? MF_CHECKED : MF_UNCHECKED);
    InvalidateRect(app->hwnd, NULL, TRUE);
}

// Default highlight theme colors
static HighlightTheme g_default_theme = {
    "default",
    RGB(86, 156, 214),     // keyword - blue
    RGB(206, 145, 120),    // string - orange
    RGB(106, 153, 85),     // number - green
    RGB(87, 166, 74),      // comment - green
    RGB(78, 201, 176),     // type - cyan
    RGB(220, 220, 170),    // function - yellow
    RGB(180, 180, 180),    // operator - gray
    RGB(86, 156, 214),    // tag - blue
    RGB(186, 140, 170),    // attribute - purple
    RGB(156, 220, 254),    // property - light blue
    RGB(220, 220, 220)     // normal - light gray
};

COLORREF get_token_color(HighlightTokenType type) {
    switch (type) {
        case HL_TOKEN_KEYWORD: return g_default_theme.keyword;
        case HL_TOKEN_STRING: return g_default_theme.string;
        case HL_TOKEN_NUMBER: return g_default_theme.number;
        case HL_TOKEN_COMMENT: return g_default_theme.comment;
        case HL_TOKEN_TYPE: return g_default_theme.type;
        case HL_TOKEN_FUNCTION: return g_default_theme.function;
        case HL_TOKEN_OPERATOR: return g_default_theme.operator_;
        case HL_TOKEN_TAG: return g_default_theme.tag;
        case HL_TOKEN_ATTRIBUTE: return g_default_theme.attribute;
        case HL_TOKEN_PROPERTY: return g_default_theme.property;
        default: return g_default_theme.normal;
    }
}

LanguageType detect_language(const char *filename) {
    if (!filename) return LANG_NONE;

    const char *ext = strrchr(filename, '.');
    if (!ext) return LANG_NONE;
    ext++;

    // JSON
    if (strcmp(ext, "json") == 0) return LANG_JSON;
    // XML
    if (strcmp(ext, "xml") == 0) return LANG_XML;
    // HTML
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return LANG_HTML;
    // CSS
    if (strcmp(ext, "css") == 0) return LANG_CSS;
    // JavaScript
    if (strcmp(ext, "js") == 0) return LANG_JAVASCRIPT;
    // Python
    if (strcmp(ext, "py") == 0) return LANG_PYTHON;
    // Markdown
    if (strcmp(ext, "md") == 0 || strcmp(ext, "markdown") == 0) return LANG_MARKDOWN;
    // INI
    if (strcmp(ext, "ini") == 0 || strcmp(ext, "cfg") == 0 || strcmp(ext, "conf") == 0) return LANG_INI;
    // YAML
    if (strcmp(ext, "yaml") == 0 || strcmp(ext, "yml") == 0) return LANG_YAML;
    // SQL
    if (strcmp(ext, "sql") == 0) return LANG_SQL;
    // C
    if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0) return LANG_C;
    // C++
    if (strcmp(ext, "cpp") == 0 || strcmp(ext, "cc") == 0 || strcmp(ext, "cxx") == 0 ||
        strcmp(ext, "hpp") == 0 || strcmp(ext, "hh") == 0) return LANG_CPP;
    // Java
    if (strcmp(ext, "java") == 0) return LANG_JAVA;

    return LANG_NONE;
}

// Simple keyword detection for syntax highlighting
static bool is_keyword(const char *word, int len) {
    static const char *keywords[] = {
        "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue",
        "return", "goto", "try", "catch", "throw", "finally", "class", "struct", "union",
        "enum", "typedef", "sizeof", "typeof", "var", "let", "const", "function", "def",
        "import", "export", "from", "as", "async", "await", "yield", "static", "public",
        "private", "protected", "virtual", "override", "final", "abstract", "extends",
        "implements", "new", "delete", "this", "super", "true", "false", "null", "nil",
        "none", "void", "bool", "int", "long", "float", "double", "char", "string",
        "byte", "short", "unsigned", "signed", "const", "inline", "extern", "register",
        "volatile", "auto", "namespace", "using", "template", "typename", "decltype",
        "auto", "nullptr", "and", "or", "not", "in", "is", "pass", "lambda", "with"
    };
    int count = sizeof(keywords) / sizeof(keywords[0]);

    for (int i = 0; i < count; i++) {
        if (strncmp(word, keywords[i], len) == 0 && keywords[i][len] == '\0') {
            return true;
        }
    }
    return false;
}

void highlight_line(const char *line, int len, LanguageType lang, HighlightToken *tokens, int *token_count) {
    *token_count = 0;
    if (!line || len <= 0) return;

    int i = 0;

    // Handle line comments
    if (lang == LANG_PYTHON || lang == LANG_JAVASCRIPT || lang == LANG_C ||
        lang == LANG_CPP || lang == LANG_JAVA || lang == LANG_SQL) {
        // Check for // comment
        if (len >= 2 && line[0] == '/' && line[1] == '/') {
            tokens[0].type = HL_TOKEN_COMMENT;
            tokens[0].start = 0;
            tokens[0].end = len;
            *token_count = 1;
            return;
        }
        // Check for -- comment (SQL)
        if (lang == LANG_SQL && len >= 2 && line[0] == '-' && line[1] == '-') {
            tokens[0].type = HL_TOKEN_COMMENT;
            tokens[0].start = 0;
            tokens[0].end = len;
            *token_count = 1;
            return;
        }
        // Check for # comment (Python, etc.)
        if (len >= 1 && line[0] == '#') {
            tokens[0].type = HL_TOKEN_COMMENT;
            tokens[0].start = 0;
            tokens[0].end = len;
            *token_count = 1;
            return;
        }
    }

    // Tokenize the line
    while (i < len && *token_count < 32) {
        char c = line[i];

        // Skip whitespace
        if (c == ' ' || c == '\t') {
            i++;
            continue;
        }

        // String literals
        if (c == '"' || c == '\'') {
            char quote = c;
            int start = i;
            i++;
            while (i < len && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++; // closing quote
            tokens[*token_count].type = HL_TOKEN_STRING;
            tokens[*token_count].start = start;
            tokens[*token_count].end = i;
            (*token_count)++;
            continue;
        }

        // Numbers
        if ((c >= '0' && c <= '9') || (c == '.' && i + 1 < len && line[i + 1] >= '0' && line[i + 1] <= '9')) {
            int start = i;
            while (i < len && ((line[i] >= '0' && line[i] <= '9') || line[i] == '.' ||
                              line[i] == 'x' || line[i] == 'X' || line[i] == 'e' || line[i] == 'E' ||
                              line[i] == 'f' || line[i] == 'F' || line[i] == 'l' || line[i] == 'L' ||
                              (line[i] >= 'a' && line[i] <= 'f') || (line[i] >= 'A' && line[i] <= 'F'))) {
                i++;
            }
            tokens[*token_count].type = HL_TOKEN_NUMBER;
            tokens[*token_count].start = start;
            tokens[*token_count].end = i;
            (*token_count)++;
            continue;
        }

        // HTML/XML tags
        if ((lang == LANG_HTML || lang == LANG_XML || lang == LANG_JSON) && c == '<') {
            int start = i;
            while (i < len && line[i] != '>') i++;
            if (i < len) i++;
            tokens[*token_count].type = HL_TOKEN_TAG;
            tokens[*token_count].start = start;
            tokens[*token_count].end = i;
            (*token_count)++;
            continue;
        }

        // Identifiers and keywords
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '@') {
            int start = i;
            while (i < len && ((line[i] >= 'a' && line[i] <= 'z') || (line[i] >= 'A' && line[i] <= 'Z') ||
                              (line[i] >= '0' && line[i] <= '9') || line[i] == '_' || line[i] == '@')) {
                i++;
            }
            int word_len = i - start;

            // Check for keywords
            if (is_keyword(line + start, word_len)) {
                tokens[*token_count].type = HL_TOKEN_KEYWORD;
            } else if (i < len && line[i] == '(') {
                // Function call
                tokens[*token_count].type = HL_TOKEN_FUNCTION;
            } else {
                tokens[*token_count].type = HL_TOKEN_NORMAL;
            }
            tokens[*token_count].start = start;
            tokens[*token_count].end = i;
            (*token_count)++;
            continue;
        }

        // Operators
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '=' ||
            c == '<' || c == '>' || c == '!' || c == '&' || c == '|' || c == '^' || c == '~') {
            int start = i;
            // Handle multi-character operators
            while (i < len && (line[i] == '+' || line[i] == '-' || line[i] == '*' || line[i] == '/' ||
                              line[i] == '%' || line[i] == '=' || line[i] == '<' || line[i] == '>' ||
                              line[i] == '!' || line[i] == '&' || line[i] == '|' || line[i] == '^' ||
                              line[i] == '~')) {
                i++;
            }
            tokens[*token_count].type = HL_TOKEN_OPERATOR;
            tokens[*token_count].start = start;
            tokens[*token_count].end = i;
            (*token_count)++;
            continue;
        }

        // CSS properties (e.g., "color:", "background:")
        if (lang == LANG_CSS && i > 0 && line[i] == ':' && line[i-1] != ':') {
            // Check if previous word is a property
            int j = i - 1;
            while (j > 0 && (line[j] == ' ' || line[j] == '\t')) j--;
            int end = j + 1;
            while (j > 0 && ((line[j] >= 'a' && line[j] <= 'z') || (line[j] >= 'A' && line[j] <= 'Z'))) {
                j--;
            }
            j++;
            // Check if it looks like a property name (ends with :)
            if (end > j && line[end-1] == ':') {
                tokens[*token_count].type = HL_TOKEN_PROPERTY;
                tokens[*token_count].start = j;
                tokens[*token_count].end = end;
                (*token_count)++;
            }
        }

        i++;
    }
}

// ============================================================================
// Backup System Functions
// ============================================================================

bool app_backup_create(App *app, const char *filepath) {
    (void)app;
    
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (!tab) return false;
    
    if (!filepath || strcmp(filepath, "Untitled") == 0) {
        return false;
    }
    
    // Get buffer data
    int len = buffer_length(&tab->editor.buffer);
    if (len <= 0) return false;
    
    char *data = buffer_get_text(&tab->editor.buffer, 0, (TextPos)len);
    if (!data) return false;
    
    bool result = backup_create(filepath, data, (size_t)len);
    free(data);
    
    return result;
}

bool app_backup_restore(App *app, const char *filepath, int version_index) {
    (void)app;
    
    if (!filepath) return false;
    
    void *data = NULL;
    size_t size = 0;
    
    BackupList *list = backup_list(filepath);
    if (!list) return false;
    
    bool result = false;
    if (version_index >= 0 && version_index < list->count) {
        if (backup_restore(list, version_index, &data, &size)) {
            // Load into active tab
            Tab *tab = tab_manager_get_active(&app->tab_mgr);
            if (tab) {
                buffer_delete(&tab->editor.buffer, 0, buffer_length(&tab->editor.buffer));
                buffer_insert(&tab->editor.buffer, 0, (const char *)data, (int)size);
                result = true;
            }
            free(data);
        }
    }
    
    backup_list_free(list);
    return result;
}

const char* app_backup_get_dir(App *app) {
    (void)app;
    return backup_get_directory();
}

int app_backup_get_count(App *app, const char *filepath) {
    (void)app;
    return backup_count(filepath);
}

bool app_backup_enable(App *app, bool enable) {
    app->backup_enabled = enable;
    return true;
}

// ============================================================================
// Plugin System Functions
// ============================================================================

bool app_plugins_init(App *app) {
    (void)app;
    plugin_init();
    return true;
}

void app_plugins_shutdown(App *app) {
    (void)app;
    plugin_shutdown();
}

bool app_plugins_load(App *app) {
    (void)app;
    return plugin_load_all();
}

Plugin* app_plugin_get(App *app, const char *plugin_id) {
    (void)app;
    return plugin_get(plugin_id);
}

// ============================================================================
// Settings Export/Import Functions
// ============================================================================

bool app_settings_export(App *app, const char *filepath) {
    (void)app;
    return settings_export_to_file(filepath, SETTINGS_CATEGORY_ALL);
}

bool app_settings_import(App *app, const char *filepath) {
    (void)app;
    return settings_import_from_file(filepath, SETTINGS_CATEGORY_ALL);
}
