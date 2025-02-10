// XXX: Reduce compilation time?
// XXX: Change file name? The functionalities herein initialise mold 
// data, which includes non-graphical data. As such, `gfx` is a 
// misleading file name.
#include <WinDef.h>
#include <wingdi.h>

#include "global.h"
#include "gfx.h"

HBITMAP allocGfx(HDC sourceDc, BITMAPINFO *bi, unsigned int w, 
        unsigned int h) {
    
    LONG tempWidth, tempHeight;
    HBITMAP hb;
    BITMAPINFOHEADER *bih = &bi->bmiHeader;
    
    // XXX: This temp strategy is terrible, but is there better?
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

// XXX: Remove when debugging is over
#include <stdio.h>

#include <fileapi.h>
#include <handleapi.h>
#include "global_dict.h"

// XXX: Remove when sprites display actual pixel data.
#define PIXELS 16

// XXX: Finish implementing. Should initialise all molds in the 
// struct from a file.
MirageError formatAtlas(sMoldDirectory *md, HDC sourceDc, 
        BITMAPINFO *bi) {
    
    // XXX: Clean up this variable mess.
    HANDLE hFile;
    char buffer[64];
    unsigned long bytes;
    
    // XXX: Remove once sprites have actual pixel data to display.
    sPixel pelData[PIXELS][PIXELS];
    long tempWidth, tempHeight;
    HBITMAP hb;
    BITMAPINFOHEADER *bih;
    
    hFile = CreateFile(DIR_MOLDINFO, 
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    
    // XXX: Fix to read proper amount of bytes. There is an 
    // indeterminate amount of molds to read.
    if (hFile == INVALID_HANDLE_VALUE
            || !ReadFile(hFile, buffer, ARRAY_ELEMENTS(buffer), &bytes, 
            NULL)) {
        return !MIRAGE_OK;
        
    }
    
    // The process should not worry about whether a file closed 
    // properly or not.
    CloseHandle(hFile);
    
    bih = &bi->bmiHeader;
    
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
    
    // XXX: Only initialises the mold that the player will use. 
    // Add all other molds that sprites will use.
    // XXX: The directory mold identifier of zero is not necessarily 
    // the one for the player.
    // XXX: Actually load the bitmap for the animation frames.
    md->data[0].hb = (void*) hb;
    md->data[0].w = PIXELS;
    md->data[0].h = PIXELS;
    md->data[0].maxSpeed = 4;
    md->data[0].subAccel = 50;
    return MIRAGE_OK;
}