// XXX: Reduce compilation time?
// XXX: Change file name? The functionalities herein initialise mold 
// data, which includes non-graphical data. As such, `gfx` is a 
// misleading file name.
#include <WinDef.h>
#include <wingdi.h>
#include <winuser.h>

#include "global.h"
#include "gfx.h"

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

static int loadMoldDirectory(sMoldDirectory *dstMold, sPixel **pelBuffer, 
    HANDLE hMoldInfoFile, HDC dstMemDc, BITMAPINFO *bi);
static int loadSprite(sMold *dstMold, sPixel *pelBuffer, 
    char const (*name)[8], HDC dstMemDc, BITMAPINFO *bi);
static int decodeGfx(sPixel *dst, HANDLE hf, unsigned long stridePixels);

int initMoldDirectory(sMoldDirectory *dstMold, HDC dstMemDc, 
        BITMAPINFO *bi) {
    HANDLE hMoldInfoFile;
    sPixel *buffer;
    int error;
    
    hMoldInfoFile = CreateFile(DIR_MOLDINFO, 
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hMoldInfoFile == INVALID_HANDLE_VALUE) {
        PANIC("The process could not find actor mold data. "
            "Expected a file in \""DIR_MOLDINFO"\".", 
            MIRAGE_NO_MOLDINFO);
        return 1;
        
    }
    
    buffer = NULL;
    error = loadMoldDirectory(dstMold, &buffer, hMoldInfoFile, dstMemDc, bi);
    
    // The `loadMoldDirectory` function call cannot close the file 
    // containing mold information.
    CloseHandle(hMoldInfoFile);
    
    // The call to the `loadMoldDirectory` function is not responsible 
    // for deallocating buffer graphic data.
    if (!VirtualFree(buffer, 0, MEM_RELEASE)) {
        PANIC("The process failed to free buffer memory for graphic data.",
            MIRAGE_HEAP_FREE_FAIL);
        return 1;
        
    }
    
    if (error) {
        
        // The call to the `loadMoldDirectory` function is responsible 
        // for posting the quit message.
        return 1;
        
    }
    
    return 0;
}

#define CHUNK_PIXELS 64U

static int loadMoldDirectory(sMoldDirectory *dstMold, sPixel **pelBuffer, 
        HANDLE hMoldInfoFile, HDC dstMemDc, BITMAPINFO *bi) {
    struct {
        unsigned long io, maxPel;
    } bytes;
    char buffer[64];
    struct {
        char name[8];
        unsigned char braces, moldIndex, characters;
        char foundDatum;
        unsigned short number;
        enum {
            MOLDINFO_MOLDS,
            MOLDINFO_ENTRY,
            MOLDINFO_NAME,
            MOLDINFO_WIDTH,
            MOLDINFO_HEIGHT,
            MOLDINFO_SPEED,
            MOLDINFO_ACCEL,
            MOLDINFO_FRAMES,
            MOLDINFO_DATUM
        } search;
    } state = { { 0 }, 0, 0, 0, 0, 0, MOLDINFO_MOLDS };
    
    bytes.maxPel = 0;
    do {
        unsigned int byteIndex;
        
        if (!ReadFile(hMoldInfoFile, buffer, ARRAY_ELEMENTS(buffer), 
                &bytes.io, NULL)) {
            PANIC("The process unexpectedly cannot read \""DIR_MOLDINFO"\".", 
                MIRAGE_INTERRUPT_MOLDINFO);
            
            // The caller function is responsible for closing the mold 
            // information file.
            break;
            
        }
        
        for (byteIndex = 0; byteIndex < bytes.io; ++byteIndex) {
            char const byte = buffer[byteIndex];
            
            switch (byte) {
                case '{': {
                    if (state.braces >= 2U) {
                        break;
                
                case '}':
                        // Assume that the scope level is non-zero.
                        
                        if (state.braces-- == 1) {
                            unsigned char temp;
                            
                            if (state.moldIndex-- == 0) {
                                
                                // The process ignores any data 
                                // following the last mold entry.
                                return 0;
                                
                            }
                            
                            temp = state.moldIndex;
                            
                            // Wipe out all mold information before 
                            // beginning to process the next mold 
                            // entry.
                            memset(&state, 0x00, sizeof state);
                            state.moldIndex = temp;
                            state.search = MOLDINFO_ENTRY;
                            
                        } else {
                            static unsigned char const datum[] = {
                                MOLDINFO_WIDTH,
                                MOLDINFO_HEIGHT,
                                MOLDINFO_FRAMES,
                                MOLDINFO_NAME,
                                MOLDINFO_SPEED,
                                MOLDINFO_ACCEL
                            };
                            
                            if (state.number < ARRAY_ELEMENTS(datum)) {
                                state.search = datum[state.number];
                                
                            }
                            
                            state.number = 0U;
                            state.foundDatum = 0;
                            continue;
                            
                        }
                        
                    } else {
                        state.search = MOLDINFO_DATUM;
                        ++state.braces;
                        
                    }
                }
                // Fall-through
                case '\n':
                case '\t':
                case '\r':
                case ' ': {
                    if (!state.foundDatum) {
                        continue;
                        
                    } else {
                        switch (state.search) {
                            case MOLDINFO_MOLDS: {
                                if (state.number == 0) {
                                    
                                    // Return because there are no 
                                    // mold entries to parse.
                                    return 0;
                                    
                                }
                                
                                state.moldIndex = (unsigned char) 
                                    (state.number - 1U);
                                state.search = MOLDINFO_ENTRY;
                                break;
                                
                            }
                            
                            case MOLDINFO_NAME: {
                                unsigned long const pels = (unsigned long)
                                    (dstMold->data[state.moldIndex].w
                                    * dstMold->data[state.moldIndex].h
                                    * dstMold->data[state.moldIndex].frames);
                                
                                // The amount of pixels must be a 
                                // multiple of the chunk size.
                                if (pels==0 || pels%CHUNK_PIXELS!=0) {
                                    PANIC("An invalid mold entry is in "
                                        "the mold information file.",
                                        MIRAGE_INVALID_MOLDENTRY);
                                    return 1;
                                    
                                }
                                
                                if (pels > bytes.maxPel) {
                                    sPixel *p = *pelBuffer;
                                    
                                    // The process should not bother 
                                    // checking whether it freed 
                                    // memory correctly or not.
                                    if (p != NULL) {
                                        VirtualFree(p, 0, MEM_RELEASE);
                                        
                                    }
                                    
                                    bytes.maxPel = (unsigned int)
                                        (pels * sizeof*p);
                                    p = VirtualAlloc(NULL, 
                                        bytes.maxPel, 
                                        MEM_COMMIT, 
                                        PAGE_READWRITE);
                                    if (p == NULL) {
                                        PANIC("The process failed to reserve "
                                            "heap memory for buffering pixel "
                                            "data.", 
                                            MIRAGE_HEAP_ALLOC_FAIL);
                                        return 1;
                                        
                                    }
                                    
                                    // Commit the changes to the 
                                    // current allocation for storing 
                                    // sprite pixel data.
                                    *pelBuffer = p;
                                    
                                }
                                
                                if (loadSprite(
                                        &dstMold->data[state.moldIndex], 
                                        *pelBuffer, &state.name, dstMemDc, 
                                        bi)) {
                                    
                                    // The call to the `loadSprite` 
                                    // function is responsible for 
                                    // posting the quit message.
                                    return 1;
                                    
                                }
                                break;
                                
                            }
                            
                            case MOLDINFO_WIDTH: {
                                dstMold->data[state.moldIndex].w = 
                                    (unsigned char) state.number;
                                break;
                                
                            }
                            
                            case MOLDINFO_HEIGHT: {
                                dstMold->data[state.moldIndex].h = 
                                    (unsigned char) state.number;
                                break;
                                
                            }
                            
                            case MOLDINFO_SPEED: {
                                dstMold->data[state.moldIndex]
                                    .maxSpeed = (unsigned char) 
                                    state.number;
                                break;
                                
                            }
                            
                            case MOLDINFO_ACCEL: {
                                dstMold->data[state.moldIndex]
                                    .subAccel = (signed char) 
                                    state.number;
                                break;
                                
                            }
                            
                            case MOLDINFO_FRAMES: {
                                dstMold->data[state.moldIndex].frames = 
                                    (unsigned char) state.number;
                                break;
                                
                            }
                            
                            case MOLDINFO_DATUM:
                            case MOLDINFO_ENTRY:
                            default: {
                                break;
                                
                            }
                        }
                        
                    }
                    state.number = 0U;
                    state.foundDatum = 0;
                    continue;
                    
                }
                
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': {
                    state.number = (unsigned short)
                        (state.number*10U);
                    state.number = (unsigned short)
                        (state.number + (byte-'0'));
                    state.foundDatum = 1;
                    continue;
                    
                }
                
                case 'a': case 'A':
                case 'b': case 'B':
                case 'c': case 'C':
                case 'd': case 'D':
                case 'e': case 'E':
                case 'f': case 'F':
                case 'g': case 'G':
                case 'h': case 'H':
                case 'i': case 'I':
                case 'j': case 'J':
                case 'k': case 'K':
                case 'l': case 'L':
                case 'm': case 'M':
                case 'n': case 'N':
                case 'o': case 'O':
                case 'p': case 'P':
                case 'q': case 'Q':
                case 'r': case 'R':
                case 's': case 'S':
                case 't': case 'T':
                case 'u': case 'U':
                case 'v': case 'V':
                case 'w': case 'W':
                case 'x': case 'X':
                case 'y': case 'Y':
                case 'z': case 'Z': {
                    if (state.search!=MOLDINFO_NAME
                            || state.characters
                            >=ARRAY_ELEMENTS(state.name) - 1) {
                        // Include the null terminal.
                        
                        break;
                        
                    }
                    state.name[state.characters++] = byte;
                    state.foundDatum = 1;
                    continue;
                    
                }
                
                default:
            }
            
            PANIC("The mold information file \""DIR_MOLDINFO
                "\" is invalid.", MIRAGE_INVALID_MOLDINFO);
            return 1;
            
        }
    } while (bytes.io == sizeof buffer);
    
    if (state.moldIndex != 0) {
        PANIC("Mismatch between amount of molds and entries in "
            "\""DIR_MOLDINFO"\".", MIRAGE_INVALID_MOLDINFO);
        return 1;
        
    }
    
    return 0;
}

static int loadSprite(sMold *dstMold, sPixel *pelBuffer, 
        char const (*name)[8], HDC dstMemDc, BITMAPINFO *bi) {
    HANDLE hGraphicFile;
    BITMAPINFOHEADER *bih = &bi->bmiHeader;
    signed long const tempW = bih->biWidth, tempH = bih->biHeight;
    char const *src;
    char *write;
    int error;
    unsigned int characters;
    char root[] = DIR_SPRITE, suffix[] = "\0\0\0\0\0\0\0\0\0\0\0\0";
    char const extension[] = EXTENSION_GFX;
    char dir[sizeof root + sizeof suffix + sizeof extension];
    
    root[ARRAY_ELEMENTS(root) - 1] = '\\';
    
    for (write = &suffix[0], src = *name, characters = 0;
            *src != '\0'; 
            ++write, ++src, ++characters) {
        *write = *src;
    }
    memcpy(dir, root, sizeof root);
    memcpy(dir + sizeof root, suffix, characters * sizeof*suffix);
    memcpy(dir + sizeof root + characters*sizeof*suffix, extension, 
        sizeof extension);
    
    hGraphicFile = CreateFile(dir, 
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    
    if (hGraphicFile == INVALID_HANDLE_VALUE) {
        PANIC("The process could not locate sprite data.",
            MIRAGE_NO_GFX_SPRITE);
        return 1;
        
    }
    
    do {
        unsigned long pixels;
        sSprite s;
        
        bih->biWidth = dstMold->w;
        bih->biHeight = dstMold->h * dstMold->frames;
        
        pixels = (unsigned long)(bih->biWidth * bih->biHeight);
        if (decodeGfx(pelBuffer, hGraphicFile, pixels)) {
            PANIC("The process failed to load sprite data.",
                MIRAGE_LOAD_GFX_FAIL);
            error = 1;
            break;
            
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
            if (s.color == NULL) {
                PANIC("The process failed to create a sprite graphic.",
                    MIRAGE_LOAD_SPRITE_FAIL);
                error = 1;
                break;
                
            }
            
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
                error = 1;
                break;
                
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
                error = 1;
                break;
                
            }
            
            dstMold->s = s;
            
            error = 0;
            
        }
    } while (0);
    
    bih->biWidth = tempW;
    bih->biHeight = tempH;
    CloseHandle(hGraphicFile);
    return error;
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
        OPEN_EXISTING,
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
    
    return hb;
}

#define RGB24_PIXEL_BYTES 3

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
    
    return 0;
}