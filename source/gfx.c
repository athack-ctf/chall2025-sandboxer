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
    
    memset(&pelData, 0x55, sizeof(pelData));
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

// XXX: Remove once debugging is over.
#include <stdio.h>

HBITMAP allocGfx(HDC sourceDc, BITMAPINFO *bi, unsigned int w, 
        unsigned int h) {
    
    LONG tempWidth, tempHeight;
    HBITMAP hb;
    BITMAPINFOHEADER *bih = &bi->bmiHeader;
    
    // XXX: This temp strategy is terrible.
    tempWidth = bih->biWidth;
    tempHeight = bih->biHeight;
    bih->biWidth = (LONG) w;
    bih->biHeight = (LONG) h;
    
    hb = CreateDIBitmap(sourceDc,
        bih,
        0, // Do not initialize pixel data.
        NULL, // There is no initialization data as a result.
        bi,
        DIB_RGB_COLORS);
    
    bih->biWidth = tempWidth;
    bih->biHeight = tempHeight;
    
    return hb;
}