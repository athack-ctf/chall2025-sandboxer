// XXX: Reduce compilation time?
#include <WinDef.h>
#include <wingdi.h>

#include "gfx.h"

#define PIXELS 32

HBITMAP loadGfx(HDC sourceDc, BITMAPINFO *bi) {
    sPixel pelData[PIXELS][PIXELS];
    LONG tempWidth, tempHeight;
    HBITMAP hb;
    BITMAPINFOHEADER *bih = &bi->bmiHeader;
    
    memset(&pelData, 0xFF, sizeof(pelData));
    tempWidth = bih->biWidth;
    tempHeight = bih->biHeight;
    bih->biWidth = PIXELS;
    bih->biHeight = PIXELS;
    
    hb = CreateDIBitmap(sourceDc,
        bih,
        CBM_INIT,
        &pelData,
        bi,
        DIB_RGB_COLORS);
    
    bih->biWidth = tempWidth;
    bih->biHeight = tempHeight;
    
    return hb;
}