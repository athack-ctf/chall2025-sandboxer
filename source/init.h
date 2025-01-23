#ifndef _HEADER_INIT

HWND constructWindow(
    LRESULT (*windowProcedure)(HWND, unsigned int, WPARAM, LPARAM));

#define _HEADER_INIT
#endif