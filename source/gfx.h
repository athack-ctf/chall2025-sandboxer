#ifndef _HEADER_GFX

#define VIEWPORT_HEIGHT 288
#define VIEWPORT_WIDTH 512
#define VIEWPORT_BPP (8*sizeof(sPixel))

typedef struct {
    unsigned char r, g, b, a;
} sPixel;

HBITMAP loadGfx(HDC sourceDc, BITMAPINFO *bi);

HBITMAP allocGfx(HDC sourceDc, BITMAPINFO *bi, unsigned int w, 
        unsigned int h);

#define _HEADER_GFX
#endif