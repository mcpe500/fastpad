#include "app.h"
#include "log.h"
#include <windows.h>

// Exception handler to capture crashes
LONG WINAPI crash_handler(EXCEPTION_POINTERS *ExceptionInfo) {
    #ifdef DEV_BUILD
    log_error("!!! CRASH DETECTED !!!");
    log_error("Exception Code: 0x%08X", ExceptionInfo->ExceptionRecord->ExceptionCode);
    log_error("Faulting Address: 0x%p", ExceptionInfo->ExceptionRecord->ExceptionAddress);
    log_error("The application has crashed. Please copy the log file and send it to the developer.");
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
