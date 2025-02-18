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

static HBITMAP decodeGfx(HANDLE hf, BITMAPINFO const *bi, HDC hdc);

// XXX: Finish implementing. Should initialise all molds in the 
// struct from a file.
MirageError formatAtlas(sMoldDirectory *md, HDC sourceDc, 
        BITMAPINFO *bi) {
    
    // XXX: Clean up this variable mess.
    HANDLE hFile;
    char buffer[64];
    unsigned long bytes;
    
    signed long tempWidth, tempHeight;
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
        CloseHandle(hFile);
        return MIRAGE_NO_MOLDINFO;
        
    }
    
    // The process should not worry about whether a file closed 
    // properly or not.
    CloseHandle(hFile);
    
    bih = &bi->bmiHeader;
    
    tempWidth = bih->biWidth;
    tempHeight = bih->biHeight;
    
    // XXX: Change dimensions according to the mold information 
    // instead of hardcoding data.
    bih->biWidth = 16;
    bih->biHeight = 224;
    
    // XXX: Repeat file read for every sprite.
    hFile = CreateFile(DIR_GFX_GUY_NOTHING, 
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        return MIRAGE_NO_GFX_SPRITE;
        
    }
    hb = decodeGfx(hFile, bi, sourceDc);
    
    // The process should not bother with recovering from an 
    // unsuccessful handle closure.
    CloseHandle(hFile);
    
    bih->biWidth = tempWidth;
    bih->biHeight = tempHeight;
    
    if (hb == NULL) {
        return MIRAGE_LOAD_GFX_FAIL;
        
    }
    
    // XXX: Only initialises the mold that the player will use. 
    // Add all other molds that sprites will use.
    // XXX: The directory mold identifier of zero is not necessarily 
    // the one for the player.
    // XXX: Actually load the bitmap for the animation frames.
    md->data[0].hb = (void*) hb;
    md->data[0].w = 16;
    md->data[0].h = 17;
    md->data[0].maxSpeed = 4;
    md->data[0].subAccel = 60;
    return MIRAGE_OK;
}

#include <memoryapi.h>

#define RGB24_PIXEL_BYTES 3

static int translateBody(sPixel *dst, HANDLE hf, sPixel const *color);

// XXX: Place `decodeGfx` before `translateBody`?
static HBITMAP decodeGfx(HANDLE hf, BITMAPINFO const *bi, HDC hdc) {
    struct {
        unsigned long io, pelData;
    } bytes;
    unsigned long w, h, i;
    sPixel *pixelBuffer;
    HBITMAP hb;
    sPixel color[8];
    
    // There can be at most seven different colours.
    unsigned char byteBuffer[1 + RGB24_PIXEL_BYTES*7], colors;
    
    w = (unsigned long)bi->bmiHeader.biWidth;
    h = (unsigned long)bi->bmiHeader.biHeight;
    bytes.pelData = (unsigned long)(w*h*sizeof(*pixelBuffer));
    pixelBuffer = VirtualAlloc(NULL, bytes.pelData, MEM_COMMIT,
        PAGE_READWRITE);
    if (pixelBuffer == NULL) {
        return NULL;
        
    }
    
    // XXX: Remove once graphics work
    memset(pixelBuffer, 0x55, bytes.pelData);
    
    // Begin by reading the amount of colours in the palette header.
    // The buffer to store the amount of bytes that the operation can 
    // be null. However, Windows 7 does not support this case.
    if (!ReadFile(hf, &colors, sizeof colors, &bytes.io, NULL)) {
        return NULL;
        
    }
    
    // The palette header describes pixel colours as RGB24. This 
    // format stores a pixel with three bytes.
    if (!ReadFile(hf, byteBuffer, (unsigned long) (RGB24_PIXEL_BYTES*colors), 
            &bytes.io, NULL)) {
        return NULL;
        
    }
    
    // The pixel bearing the index of zero is the transparent pixel. 
    // The alpha distinguishes opaque colours from transparency.
    memset(&color[0], 0x00, sizeof color[0]);
    for (i = 1; i <= colors; ++i) {
        color[i].a = 0xFF;
        color[i].r = byteBuffer[RGB24_PIXEL_BYTES*i + 0];
        color[i].g = byteBuffer[RGB24_PIXEL_BYTES*i + 1];
        color[i].b = byteBuffer[RGB24_PIXEL_BYTES*i + 2];
    }
    
    if (translateBody(pixelBuffer, hf, color)) {
        hb = NULL;
        
    } else {
        hb = CreateDIBitmap(hdc, 
            &bi->bmiHeader,
            CBM_INIT,
            pixelBuffer,
            bi,
            DIB_RGB_COLORS);
        
    }
    
    // The process should not bother checking whether it closed the 
    // file correctly or not.
    CloseHandle(hf);
    
    // The process should not bother checking that the operating 
    // system deallocated memory without issue.
    VirtualFree(pixelBuffer, bytes.pelData, MEM_RELEASE);
    return hb;
}

#define CHUNK_PIXELS 64
#define MASK_THERE_IS_TAIL 0x10
#define MASK_COLOR 0xE0
#define SHIFT_COLOR 5

#define SINGLE_MASK_LENGTH 0x0F
#define SINGLE_SHIFT_LENGTH 0
#define DOUBLE_HIGH_HEAD_SHIFT 2
#define DOUBLE_LOW_HEAD_SHIFT 6
#define DOUBLE_MASK_HEAD_LENGTH 0xC0
#define DOUBLE_MASK_TAIL_LENGTH 0x3F
#define DOUBLE_SHIFT_TAIL_LENGTH 0

static int translateBody(sPixel *dst, HANDLE hf, sPixel const *color) {
    struct {
        sPixel *head, *tail;
    } brush;
    struct {
        unsigned long io, chunk;
    } bytes;
    struct {
        unsigned char pixelsLeftInChunk, colorIndex;
        struct {
            unsigned char head, tail;
        } pixels;
        char thereIsTail;
    } state;
    
    // The buffer size for storing the contents of the graphic file 
    // is arbitrary.
    unsigned char buffer[128];
    
    // The brushes are responsible for pointing to the next pixel to 
    // write over.
    brush.head = dst;
    brush.tail = dst + CHUNK_PIXELS-1;
    state.pixelsLeftInChunk = CHUNK_PIXELS;
    state.pixels.tail = 0;
    
    // The cursor in the file must be at the byte immediately the 
    // palette header.
    do {
        unsigned long bufferIndex;
        
        if (!ReadFile(hf, buffer, sizeof buffer, &bytes.io, 
                NULL)) {
            return 1;
            
        }
        
        printf("read %lu bytes\n", bytes.io);
        
        for (bufferIndex = 0; bufferIndex < bytes.io; ) {
            if (state.pixelsLeftInChunk != 0) {
                unsigned char const byte = buffer[bufferIndex++];
                printf("read byte %x\n", byte);
                if (state.thereIsTail) {
                    state.pixels.head = (unsigned char)(state.pixels.head 
                        << DOUBLE_HIGH_HEAD_SHIFT)
                        | ((byte&DOUBLE_MASK_HEAD_LENGTH)
                        >> DOUBLE_LOW_HEAD_SHIFT);
                    state.pixels.tail = (unsigned char)
                        (byte&DOUBLE_MASK_TAIL_LENGTH) 
                        >> DOUBLE_SHIFT_TAIL_LENGTH;
                    
                    state.pixelsLeftInChunk = (unsigned char)
                        (state.pixelsLeftInChunk - state.pixels.tail);
                    printf("counted %u tail pixels\n", state.pixels.tail);
                    while (state.pixels.tail--) {
                        *brush.tail-- = color[state.colorIndex];
                    }
                    
                    // The algorithm covered the tail portion of the 
                    // datum at this point.
                    state.thereIsTail = 0;
                    
                } else {
                    state.colorIndex = (byte&MASK_COLOR) >> SHIFT_COLOR;
                    state.pixels.head = (byte&SINGLE_MASK_LENGTH) 
                        >> SINGLE_SHIFT_LENGTH;
                    state.thereIsTail = !(byte&MASK_THERE_IS_TAIL);
                    
                    printf("infer color index %u\n", state.colorIndex);
                    
                    if (state.thereIsTail) {
                        printf("There is a tail!\n");
                        continue;
                        
                    }
                    
                }
                
                state.pixelsLeftInChunk = (unsigned char) (
                    state.pixelsLeftInChunk - state.pixels.head);
                printf("counted %u head pixels\n", state.pixels.head);
                while (state.pixels.head--) {
                    *brush.head++ = color[state.colorIndex];
                }
                printf("there are %u pixels to draw\n", state.pixelsLeftInChunk);
                
            } else {
                unsigned long long i;
                printf("parsed one chunk\nmemory dump:");
                
                for (i = 0; i < CHUNK_PIXELS; ++i) {
                    if (i%16 == 0) {
                        printf("\n");
                    }
                    printf("(0x%02x 0x%02x 0x%02x 0x%02x) ", dst[i].a,
                        dst[i].r, dst[i].g, dst[i].b);
                }
                
                
                state.pixelsLeftInChunk = CHUNK_PIXELS;
                dst += CHUNK_PIXELS;
                brush.head = dst;
                brush.tail = dst + CHUNK_PIXELS-1;
                
            }
        }
        
        
    } while (bytes.io == sizeof buffer);
    
    return 0;
}