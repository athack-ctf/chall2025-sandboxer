#ifndef _HEADER_GLOBAL

#define PANIC(MESSAGE,CODE) (MessageBox((void*)0,"Error: "MESSAGE,"Error",MB_OK|MB_ICONERROR),PostQuitMessage(CODE),(void)0)

#define VIEWPORT_FPS 60

typedef enum {
    MIRAGE_OK = 0,
    MIRAGE_INVALID_MODULE_HANDLE,
    MIRAGE_CLASS_REGISTRATION_FAIL,
    MIRAGE_WINDOW_CONSTRUCTION_FAIL,
    MIRAGE_INVALID_DC,
    MIRAGE_BACKBUFFER_INIT_FAIL,
    MIRAGE_TEXTURE_INIT_FAIL,
    MIRAGE_BACKBUFFER_WRITE_FAIL,
    MIRAGE_WINDOW_WRITE_FAIL,
    MIRAGE_INVALID_PERFCOUNTER_ARG,
    MIRAGE_INVALID_FREE
} MirageError;

#define _HEADER_GLOBAL
#endif