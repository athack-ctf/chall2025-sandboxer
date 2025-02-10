#ifndef _HEADER_GFX

#define VIEWPORT_HEIGHT 288
#define VIEWPORT_WIDTH 512
#define VIEWPORT_BPP (8*sizeof(sPixel))

typedef struct Pixel {
    unsigned char a, r, g, b;
} sPixel;

HBITMAP loadGfx(HDC sourceDc, BITMAPINFO *bi);

HBITMAP allocGfx(HDC sourceDc, BITMAPINFO *bi, unsigned int w, 
    unsigned int h);

MirageError formatAtlas(sMoldDirectory *md, HDC sourceDc, 
    BITMAPINFO *bi);

#define _HEADER_GFX
#endif