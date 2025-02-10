#ifndef _HEADER_INIT

HWND constructWindow(
    LRESULT (*windowProcedure)(HWND, unsigned int, WPARAM, LPARAM),
    sMoldDirectory *initInfo, unsigned int extraBytes);

#define _HEADER_INIT
#endif