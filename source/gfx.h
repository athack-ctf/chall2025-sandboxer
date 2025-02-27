#ifndef _HEADER_GFX

#define VIEWPORT_HEIGHT 288
#define VIEWPORT_WIDTH 512
#define VIEWPORT_BPP (8*sizeof(sPixel))

#define UNIQUE_TILES 16U

typedef struct {
    unsigned char b, g, r, a;
} sPixel;

HBITMAP initAtlas(HDC sourceDc, BITMAPINFO *bi);
HBITMAP allocGfx(HDC sourceDc, BITMAPINFO *bi, unsigned int w, 
    unsigned int h);
int initMoldDirectory(sMoldDirectory *dstMold, HDC sourceDc, BITMAPINFO *bi);

#define _HEADER_GFX
#endif