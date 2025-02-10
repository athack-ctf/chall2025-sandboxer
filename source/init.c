#include <WinDef.h>
#include <winuser.h>

#include "global.h"
#include "global_dict.h"
#include "init.h"

HWND constructWindow(
        LRESULT (*windowProcedure)(HWND, unsigned int, WPARAM, LPARAM),
        sMoldDirectory *initInfo, unsigned int extraBytes) {
    WNDCLASS wc = { 0 }; // Initialize all handles to to null pointer 
                         // values. This initialization also sets all 
                         // additional allocation byte amounts to zero.
    HWND hwnd;
    ATOM wa;
    wc.hInstance = GetModuleHandle(NULL);
    if (wc.hInstance == NULL) {
        PANIC("The process could not locate its own module handle.",
            MIRAGE_INVALID_MODULE_HANDLE);
        return NULL;
        
    }
    wc.style = CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc = windowProcedure;
    wc.lpszMenuName = WINDOW_MENU_NAME;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    
    // XXX: Is it even necessary to allocate extra bytes?
    wc.cbWndExtra = (int) extraBytes;
    
    wa = RegisterClass(&wc);
    if (wa == 0) {
        PANIC("The process failed to register the viewport window.",
            MIRAGE_CLASS_REGISTRATION_FAIL);
        return NULL;
        
    }
    
    hwnd = CreateWindow((LPCSTR)(intptr_t)wa,
        WINDOW_TITLE_NAME,
        WS_TILEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT, // Horizontal position.
        CW_USEDEFAULT, // Implicitly call the `ShowWindow` function 
                       // after setting the `WS_VISIBLE` bit.
        720,
        405,
        (HWND) NULL, // Parent window
        (HMENU) NULL, // Class menu
        wc.hInstance,
        (void*) initInfo // The pointer that the window procedure 
                         // obtains with the `WM_CREATE` message.
        );
    if (hwnd == NULL) {
        PANIC("The process failed to create a window.",
            MIRAGE_WINDOW_CONSTRUCTION_FAIL);
        return NULL;
        
    }
    
    return hwnd;
}