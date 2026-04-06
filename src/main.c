#include "app.h"
#include "log.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define IDC_COPY_LOG 2001
#define IDC_CLOSE_CRASH 2002

// Window procedure for the crash dialog
LRESULT CALLBACK CrashDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_COPY_LOG) {
                char *log_text = log_get_full_text();
                if (log_text) {
                    if (OpenClipboard(hwnd)) {
                        EmptyClipboard();
                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, strlen(log_text) + 1);
                        if (hMem) {
                            memcpy(GlobalLock(hMem), log_text, strlen(log_text) + 1);
                            GlobalUnlock(hMem);
                            SetClipboardData(CF_TEXT, hMem);
                        }
                        CloseClipboard();
                        MessageBoxA(hwnd, "Log copied to clipboard!", "Success", MB_OK | MB_ICONINFORMATION);
                    }
                    free(log_text);
                } else {
                    MessageBoxA(hwnd, "Could not read log file.", "Error", MB_OK | MB_ICONERROR);
                }
            } else if (LOWORD(wParam) == IDC_CLOSE_CRASH) {
                DestroyWindow(hwnd);
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void show_crash_dialog(DWORD code, void* addr) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = CrashDialogProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "CrashDialogClass";
    RegisterClassExA(&wc);

    char title[64];
    snprintf(title, sizeof(title), "FastPad Crash - 0x%08lX", code);
    
    HWND hwnd = CreateWindowExA(
        WS_EX_TOPMOST, "CrashDialogClass", title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 180,
        NULL, NULL, hInstance, NULL
    );

    char crash_msg[256];
    snprintf(crash_msg, sizeof(crash_msg), "A fatal error occurred.\\nException Code: 0x%08lX\\nAddress: 0x%p\\n\\nPlease click below to copy the full log.", code, addr);
    
    CreateWindowExA(0, "STATIC", crash_msg, WS_CHILD | WS_VISIBLE | SS_LEFT,
                    20, 20, 360, 60, hwnd, NULL, hInstance, NULL);

    CreateWindowExA(0, "BUTTON", "Copy Log to Clipboard", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    100, 90, 200, 30, hwnd, (HMENU)IDC_COPY_LOG, hInstance, NULL);

    CreateWindowExA(0, "BUTTON", "Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    160, 130, 80, 30, hwnd, (HMENU)IDC_CLOSE_CRASH, hInstance, NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// Exception handler to capture crashes
LONG WINAPI crash_handler(EXCEPTION_POINTERS *ExceptionInfo) {
    #ifdef DEV_BUILD
    DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;
    void* addr = ExceptionInfo->ExceptionRecord->ExceptionAddress;
    
    log_error("!!! CRASH DETECTED !!! Code: 0x%08lX at 0x%p", code, addr);
    
    show_crash_dialog(code, addr);
    #endif
    return EXCEPTION_EXECUTE_HANDLER;
}

// Application entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    
    #ifdef DEV_BUILD
    init_logging();
    log_info("Application starting (DEV BUILD)...");
    SetUnhandledExceptionFilter(crash_handler);
    #endif
    
    // Initialize app
    if (!app_init(&g_app, hInstance)) {
        #ifdef DEV_BUILD
        log_error("App initialization failed!");
        #endif
        return 1;
    }
    
    // Create main window
    if (!app_create_window(&g_app, hInstance)) {
        #ifdef DEV_BUILD
        log_error("Window creation failed!");
        #endif
        return 1;
    }
    
    // Message loop
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    #ifdef DEV_BUILD
    log_info("Message loop exited. Application shutting down.");
    close_logging();
    #endif
    
    return (int)msg.wParam;
}
