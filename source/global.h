#ifndef _HEADER_GLOBAL

#define PANIC(MESSAGE,CODE) (MessageBox((void*)0,"Error: "MESSAGE,"Error",MB_OK|MB_ICONERROR),PostQuitMessage(CODE),(void)0)
#define ARRAY_ELEMENTS(A) ~~(sizeof(A)/(sizeof(*(A))))

typedef enum {
    MIRAGE_OK = 0,
    MIRAGE_INVALID_MODULE_HANDLE,
    MIRAGE_CLASS_REGISTRATION_FAIL,
    MIRAGE_WINDOW_CONSTRUCTION_FAIL,
    MIRAGE_INVALID_DC,
    MIRAGE_BACKBUFFER_INIT_FAIL,
    MIRAGE_TEXTURE_INIT_FAIL,
    MIRAGE_BACKBUFFER_BLACK_OUT_FAIL,
    MIRAGE_BACKBUFFER_WRITE_SPRITE_FAIL,
    MIRAGE_BACKBUFFER_WRITE_DEBUG_FAIL,
    MIRAGE_BACKBUFFER_DEBUG_FAIL,
    MIRAGE_BACKBUFFER_METRIC_FAIL,
    MIRAGE_WINDOW_WRITE_FAIL,
    MIRAGE_INVALID_PERFCOUNTER_ARG,
    MIRAGE_INVALID_FREE,
    MIRAGE_INVALID_FONT,
    MIRAGE_LOST_DEBUG_FONT,
    MIRAGE_CANNOT_RENDER_FONT,
    MIRAGE_INVALID_DEBUG_PELDATA,
    MIRAGE_DEBUG_FROM_BACKBUFFER_FAIL,
    MIRAGE_DEBUG_METRIC_MISMATCH,
    MIRAGE_CANNOT_LOAD_LEVEL,
    MIRAGE_WINDOW_SET_ATTRIBUTE_FAIL,
    MIRAGE_WINDOW_GET_ATTRIBUTE_FAIL,
    MIRAGE_NO_MOLDINFO,
    MIRAGE_NO_GFX_SPRITE,
    MIRAGE_LOAD_GFX_FAIL,
    MIRAGE_LOAD_MASK_RIGHT_FAIL,
    MIRAGE_LOAD_MASK_LEFT_FAIL,
    MIRAGE_DELETE_DEFAULT_BITMAP_FAIL,
    MIRAGE_DELETE_DEFAULT_FONT_FAIL,
    MIRAGE_HEAP_ALLOC_FAIL,
    MIRAGE_NO_ATLAS,
    MIRAGE_LOAD_ATLAS_FAIL,
    MIRAGE_BACKBUFFER_WRITE_TILE_FAIL
} MirageError;

typedef struct {
    
    // Every sprite stores two bitmaps to define its graphical effect. 
    // One bitmap stores the colors of the sprite. The other bitmap 
    // stores a monochrome mask that defines which pixels are 
    // transparent.
    void *color, *maskRight, *maskLeft;
} sSprite;

typedef struct {
    
    // The process will load each all animation frames of a sprite 
    // continuously in memory. Each mold has its own set of animation 
    // frames. The mold stores this graphic data as a pair of bitmaps. 
    // One bitmap contains the color information of the bitmap. The 
    // other dictates which pixels are transparent and which are not.
    sSprite s;
    
    // Each mold encompasses all characteristics that a class of 
    // sprites have in common. All instances of the class must have 
    // the same width and height in pixels. These instances also share 
    // the same maximum horizontal speed and horizontal 
    // sub-acceleration.
    unsigned char w, h, maxSpeed;
    
    // The mob's sub-acceleration must be a signed quantity. All the 
    // sub-velocities of each mob are signed quantities. The dynamics 
    // simulator accumuates the sub-acceleration in all mobs that are 
    // accelerating.
    signed char subAccel;
} sMold;

// XXX: Add molds for other tekis, like enemies and weapons in their 
// dropped forms.
#define MOLDS 1
typedef struct {
    
    // Every mold has a unique identifying integer for it. Any such 
    // identifier's integral value must be less than the total mold 
    // count.
    sMold data[MOLDS];
} sMoldDirectory;

// XXX: Make unsigned?
#define TILE_PELS 16

#define _HEADER_GLOBAL
#endif