// XXX: Reduce compilation time?
// XXX: Change file name? The functionalities herein initialise mold 
// data, which includes non-graphical data. As such, `gfx` is a 
// misleading file name.
#include <WinDef.h>
#include <wingdi.h>
#include <winuser.h>

#include "global.h"
#include "gfx.h"

static int decodeGfx(sPixel *dst, HANDLE hf, unsigned long stridePixels);

HBITMAP allocGfx(HDC sourceDc, BITMAPINFO *bi, unsigned int w, 
        unsigned int h) {
    
    LONG tempWidth, tempHeight;
    HBITMAP hb;
    BITMAPINFOHEADER *bih = &bi->bmiHeader;
    
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
#include <memoryapi.h>
#include "global_dict.h"

// XXX: Finish implementing. Should initialise all molds in the 
// struct from a file.
int initMoldData(sMoldDirectory *dstMold, HDC dstMemDc, 
        BITMAPINFO *bi) {
    
    // XXX: Clean up this variable mess.
    HANDLE hFile;
    sPixel *pelBuffer;
    char buffer[64];
    struct {
        unsigned long io, pelData;
    } bytes;
    signed long tempWidth, tempHeight;
    unsigned long pixels;
    sSprite s;
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
            || !ReadFile(hFile, buffer, ARRAY_ELEMENTS(buffer), &bytes.io, 
            NULL)) {
        CloseHandle(hFile);
        PANIC("The process could not find actor mold data. "
            "Expected a file in \""DIR_MOLDINFO"\".", 
            MIRAGE_NO_MOLDINFO);
        return 1;
        
    }
    
    // The process should not worry about whether a file closed 
    // properly or not.
    CloseHandle(hFile);
    
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
        PANIC("The process could not locate sprite data.",
            MIRAGE_NO_GFX_SPRITE);
        return 1;
        
    }
    
    bih = &bi->bmiHeader;
    tempWidth = bih->biWidth;
    tempHeight = bih->biHeight;
    
    // XXX: Change dimensions according to the mold information 
    // instead of hardcoding data.
    bih->biWidth = 16;
    bih->biHeight = 224;
    pixels = (unsigned long)(bih->biWidth * bih->biHeight);
    
    // Implement memory reallocation system for sprite graphics.
    bytes.pelData = (unsigned long)(pixels * (unsigned long)sizeof*pelBuffer);
    pelBuffer = VirtualAlloc(NULL, bytes.pelData, MEM_COMMIT,
        PAGE_READWRITE);
    if (pelBuffer == NULL) {
        CloseHandle(hFile);
        PANIC("The process failed to reserve heap memory.",
            MIRAGE_HEAP_ALLOC_FAIL);
        return 1;
        
    }
    
    if (decodeGfx(pelBuffer, hFile, pixels)) {
        PANIC("The process failed to load sprite data.",
            MIRAGE_LOAD_GFX_FAIL);
        s.color = NULL;
        
    } else {
        char *p;
        unsigned long byteIndex, bitIndex;
        
        // Interpret as a bottom-up bitmap.
        bih->biHeight = -bih->biHeight;
        s.color = CreateDIBitmap(dstMemDc, 
            bih,
            CBM_INIT,
            pelBuffer,
            bi,
            DIB_RGB_COLORS);
        bih->biHeight = -bih->biHeight;
        
        p = (char*)pelBuffer;
        
        // Assume that eight divides the total amount of pixels in 
        // the source bitmap. This assumption does not matter here 
        // since this total is a multiple of sixty-four.
        // XXX: Pad rows to dwords?
        for (bitIndex = 0, byteIndex = 0; bitIndex < pixels; ++byteIndex) {
            signed int shift;
            char byte = 0x00;
            
            for (shift = 7; shift >= 0; --shift, ++bitIndex) {
                byte |= (char)((pelBuffer[bitIndex].a > 0) << shift);
                
            }
            p[byteIndex] = byte;
        }
        
        s.maskRight = CreateBitmap((signed int)bih->biWidth, 
            (signed int)bih->biHeight, 1, 1, pelBuffer);
        if (s.maskRight == NULL) {
            PANIC("The process failed to load the right sprite mask data.",
                MIRAGE_LOAD_MASK_RIGHT_FAIL);
            // The window destruction procedure is responsible for 
            // deallocating sprite bitmaps.
            
        }
        
        for (byteIndex = 0; byteIndex < pixels/8; ++byteIndex) {
            signed int shift = 7;
            unsigned char mirrorByte = 0x00, 
                sourceByte = (unsigned char)p[byteIndex];
            
            while (shift--) {
                char const bit = sourceByte&1;
                
                mirrorByte = (unsigned char)(mirrorByte|bit);
                sourceByte >>= 1;
                mirrorByte = (unsigned char)(mirrorByte<<1);
            }
            mirrorByte = (unsigned char)(mirrorByte|sourceByte);
            p[byteIndex] = (char)mirrorByte;
        }
        
        // XXX: Fix for sprites of larger widths.
        for (byteIndex = 0; byteIndex < pixels/8; 
                byteIndex = byteIndex + (unsigned long)(bih->biWidth/8)) {
            char const temp = p[byteIndex+1];
            p[byteIndex+1] = p[byteIndex];
            p[byteIndex] = temp;
            
        }
        
        s.maskLeft = CreateBitmap((signed int)bih->biWidth, 
            (signed int)bih->biHeight, 1, 1, pelBuffer);
        if (s.maskLeft == NULL) {
            PANIC("The process failed to load the left sprite mask data.",
                MIRAGE_LOAD_MASK_LEFT_FAIL);
            // The window destruction procedure is responsible for 
            // deallocating sprite bitmaps.
            
        }
        
    }
    
    // The process should not bother checking that the operating 
    // system deallocated memory without issue.
    VirtualFree(pelBuffer, bytes.pelData, MEM_RELEASE);
    
    // The process should not bother with recovering from an 
    // unsuccessful handle closure.
    CloseHandle(hFile);
    
    bih->biWidth = tempWidth;
    bih->biHeight = tempHeight;
    
    // XXX: Only initialises the mold that the player will use. 
    // Add all other molds that sprites will use.
    // XXX: The directory mold identifier of zero is not necessarily 
    // the one for the player.
    // XXX: Actually load the bitmap for the animation frames.
    dstMold->data[0].s = s;
    dstMold->data[0].w = 16;
    dstMold->data[0].h = 17;
    dstMold->data[0].maxSpeed = 4;
    dstMold->data[0].subAccel = 60;
    return 0;
}

#define TILES_PER_GROUP 4U
#define GROUP_PELS ~~(TILE_PELS*TILE_PELS*TILES_PER_GROUP)
#define ALL_TILE_PELS ~~(TILE_PELS*TILE_PELS*UNIQUE_TILES)

HBITMAP initAtlas(HDC dstMemDc, BITMAPINFO *bi) {
    HBITMAP hb;
    HANDLE hf;
    sPixel *pelBuffer;
    unsigned long bufferBytes = ALL_TILE_PELS 
        * (unsigned long) sizeof*pelBuffer;
    
    pelBuffer = VirtualAlloc(NULL, bufferBytes, MEM_COMMIT, PAGE_READWRITE);
    if (pelBuffer == NULL) {
        PANIC("The process failed to reserve heap memory for decoding the "
            "atlas.", MIRAGE_HEAP_ALLOC_FAIL);
        return NULL;
        
    }
    
    hf = CreateFile(DIR_ATLAS, 
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        PANIC("The process could not locate the tile atlas.",
            MIRAGE_NO_ATLAS);
        
    } else {
        signed long const tempWidth = bi->bmiHeader.biWidth, 
            tempHeight = bi->bmiHeader.biHeight;
        
        bi->bmiHeader.biWidth = TILE_PELS;
        bi->bmiHeader.biHeight = TILE_PELS*UNIQUE_TILES;
        
        if (decodeGfx(pelBuffer, hf, GROUP_PELS)) {
            PANIC("The process failed to decode the tile atlas graphic "
                "data.", MIRAGE_LOAD_ATLAS_FAIL);
            
        } else {
            
            // The resulting pixel data describes a bottom-up bitmap.
            bi->bmiHeader.biHeight = -bi->bmiHeader.biHeight;
            hb = CreateDIBitmap(dstMemDc, 
                &bi->bmiHeader,
                CBM_INIT,
                pelBuffer,
                bi,
                DIB_RGB_COLORS);
            bi->bmiHeader.biHeight = -bi->bmiHeader.biHeight;
            
        }
        CloseHandle(hf);
        
        bi->bmiHeader.biWidth = tempWidth;
        bi->bmiHeader.biHeight = tempHeight;
        
    }
    
    // The process should not bother checking that the operating 
    // system deallocated memory properly.
    VirtualFree(pelBuffer, bufferBytes, MEM_RELEASE);
    
    return hb;
}

#define RGB24_PIXEL_BYTES 3

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

static int decodeGfx(sPixel *dst, HANDLE hf, unsigned long stridePixels) {
    struct {
        sPixel *head, *tail;
    } brush;
    unsigned long ioBytes;
    
    // The file may consist of multiple repetitions of headers and 
    // graphic body data. That is, the graphic can alter its own 
    // palette data after some offset. Reading bytes from a file may 
    // load in palette header data after pixel data. As such, the 
    // state of the decoder must persist across changes in palette.
    struct {
        
        // XXX: Think of better naming and organisation.
        unsigned char pixelsLeftInChunk, colorIndex;
        char thereIsTail;
        enum {
            PROBING_COLORS,
            PROBING_BLUE,
            PROBING_GREEN,
            PROBING_RED
        } parsingHeader;
        unsigned char colors;
        struct {
            unsigned char head, tail;
            unsigned long left;
        } pixels;
        
        // The payload following the palette header can contain at most 
        // eight different colours.
        sPixel color[8];
    } state;
    
    // The buffer size for storing the contents of the graphic file 
    // is arbitrary.
    unsigned char buffer[128];
    
    // A colour index of zero represents a transparent pixel.
    memset(&state.color[0], 0x00, sizeof state.color[0]);
    
    // Set all alpha values of other colours to their maximum.
    memset(&state.color[1], 0xFF, sizeof state.color - sizeof state.color[0]);
    
    // The algorithm begins by parsing a palette header.
    state.colors = 255;
    state.colorIndex = 1;
    state.parsingHeader = PROBING_COLORS;
    
    // The brushes are responsible for pointing to the 
    // next pixel to write over.
    brush.head = dst;
    brush.tail = dst + CHUNK_PIXELS-1;
    state.pixelsLeftInChunk = CHUNK_PIXELS;
    state.pixels.tail = 0;
    state.pixels.left = stridePixels;
    state.thereIsTail = 0;
    
    do {
        unsigned int bufferIndex;
        
        // The palette header describes pixel colours as RGB24. This 
        // format stores a pixel with three bytes.
        if (!ReadFile(hf, buffer, sizeof buffer, &ioBytes, NULL)) {
            return 1;
            
        }
        
        for (bufferIndex = 0; bufferIndex < ioBytes; ) {
            unsigned char byte;
            
            if (state.pixelsLeftInChunk == 0) {
                state.pixelsLeftInChunk = CHUNK_PIXELS;
                dst += CHUNK_PIXELS;
                brush.head = dst;
                brush.tail = dst + CHUNK_PIXELS-1;
                state.pixels.left -= CHUNK_PIXELS;
                
                // Do not increment the byte index, as this phase 
                // does not get information.
                continue;
                
            } else if (state.pixels.left == 0) {
                state.colorIndex = 1;
                state.colors = 255;
                state.parsingHeader = PROBING_COLORS;
                state.pixels.left = stridePixels;
                continue;
                
            } else {
                byte = buffer[bufferIndex++];
                
            }
            
            // Allow one more color for capturing all colours in the 
            // header.
            if (state.colors) {
                switch (state.parsingHeader) {
                    case PROBING_COLORS: {
                        state.colors = byte;
                        state.parsingHeader = PROBING_BLUE;
                        continue;
                        
                    }
                    
                    case PROBING_BLUE: {
                        state.color[state.colorIndex].b = byte;
                        state.parsingHeader = PROBING_GREEN;
                        continue;
                        
                    }
                    
                    case PROBING_GREEN: {
                        state.color[state.colorIndex].g = byte;
                        state.parsingHeader = PROBING_RED;
                        continue;
                        
                    }
                    
                    case PROBING_RED: {
                        state.color[state.colorIndex].r = byte;
                        state.parsingHeader = PROBING_BLUE;
                        ++state.colorIndex;
                        --state.colors;
                        continue;
                        
                    }
                    
                    default:
                }
                
            } else {
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
                    
                    while (state.pixels.tail--) {
                        *brush.tail-- = state.color[state.colorIndex];
                    }
                    
                    // The algorithm covered the tail portion of the 
                    // datum at this point.
                    state.thereIsTail = 0;
                    
                } else {
                    state.colorIndex = (byte&MASK_COLOR) >> SHIFT_COLOR;
                    state.pixels.head = (byte&SINGLE_MASK_LENGTH) 
                        >> SINGLE_SHIFT_LENGTH;
                    state.thereIsTail = byte&MASK_THERE_IS_TAIL;
                    if (state.thereIsTail) {
                        continue;
                        
                    }
                    
                }
                
                state.pixelsLeftInChunk = (unsigned char) 
                    (state.pixelsLeftInChunk - state.pixels.head);
                
                while (state.pixels.head--) {
                    *brush.head++ = state.color[state.colorIndex];
                }
                
            }
        }
        
        // The algorithm relies on the size of the file to determine 
        // when to end.
    } while (ioBytes == sizeof buffer);
    
    // The decoding procedure may end abruptly if a chunk did not 
    // contain sixty-four pixels. The process may also prematurely end 
    // if there are mismatching amounts of pixels.
    return ioBytes == sizeof buffer;
}