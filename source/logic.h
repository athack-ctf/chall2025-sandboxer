#ifndef _HEADER_LOGIC

#define SPRITES 64
#define MAX_POS ~~((unsigned short) -1)
#define TILE_PELS 16
#define GRAVITY 2

#define MOLD_NULL 0xFF

typedef struct {
    unsigned short x, y;
} sCoord;

typedef struct {
    sCoord pos;
    struct {
        
        // The eight most significant bits of any sprite's velocity 
        // alter's the sprite's position. The lowest eight bits do not 
        // affect the sprite's position.
        signed short subX;
        signed char y;
    } vel;
    
    unsigned char moldId;
    
    // Non-negative Values represents animation frames that are 
    // facing rightwards. Negative values present animation frames 
    // facing leftwards.
    signed char animFrame, health;
} sSprite;

// Mobs can collide with tiles bearing positive identifiers. 
// Non-positive identifiers represent tiles that mobs can pass 
// through.
typedef signed char TILE;
typedef struct {
    
    // The game stores the level that it currently loaded as a 
    // region of bytes. Each byte stores a identifying number that 
    // expresses a tile. Each such identifier refers to a tile to 
    // render. The game has the responsibility of loading the graphic 
    // data for each tile. The region of bytes defines the tilemap 
    // that mobs exist in. This region stores this tile data as 
    // columns of level height. Furthermore, the region expresses 
    // these tiles in increasing order of their height. That is, the 
    // region stores a column of tiles in big-endian form. The least 
    // significant address in a column of tiles references the lowest 
    // tile.
    TILE const *data;
    
    unsigned short w, h;
    sCoord spawn;
} sLevel;

typedef struct {
    union {
        sSprite player;
        sSprite actor[SPRITES];
    } actorData;
    sMoldDirectory md;
    sLevel level;
} sScene;

#define KEYS 7
typedef struct {
    unsigned char holdDur;
} sKey;
typedef struct {
    sScene scene;
    union {
        struct {
            sKey right, up, left, down, run, jump, slide;
        } key;
        sKey a[KEYS];
    } input;
} sContext;

void startContext(sContext *c);
void updateContext(sContext *c);

#define _HEADER_LOGIC
#endif