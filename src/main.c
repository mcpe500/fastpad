#include "app.h"
#include <windows.h>

// Application entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    
    // Initialize app
    if (!app_init(&g_app, hInstance)) {
        return 1;
    }
    
    // Create main window
    if (!app_create_window(&g_app, hInstance)) {
        return 1;
    }
    
    // Message loop
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}
