#include "app.h"
#include "log.h"
#include <windows.h>
#include <stdio.h>

// Exception handler to capture crashes
LONG WINAPI crash_handler(EXCEPTION_POINTERS *ExceptionInfo) {
    #ifdef DEV_BUILD
    char err_msg[256];
    snprintf(err_msg, sizeof(err_msg), 
             "!!! FASTPAD CRASHED !!!\\n\\nException Code: 0x%08lX\\nAddress: 0x%p\\n\\nPlease copy the 'fastpad_dev.log' file next to the exe and send it to the developer.", 
             ExceptionInfo->ExceptionRecord->ExceptionCode, 
             ExceptionInfo->ExceptionRecord->ExceptionAddress);
    
    log_error("CRASH: Code 0x%08X at 0x%p", 
              ExceptionInfo->ExceptionRecord->ExceptionCode, 
              ExceptionInfo->ExceptionRecord->ExceptionAddress);
    
    MessageBoxA(NULL, err_msg, "FastPad Dev - Fatal Error", MB_OK | MB_ICONERROR);
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
