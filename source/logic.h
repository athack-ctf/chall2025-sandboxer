#ifndef _HEADER_LOGIC

typedef struct {
    unsigned short x, y;
} sCoord;

typedef struct {
    sCoord pos;
    struct {
        
        // The eight most significant bits of any actor's velocity 
        // alter's the actor's position. The lowest eight bits do not 
        // affect the actor's position.
        signed short subX;
        signed char y;
    } vel;
    
    unsigned char moldId;
    
    // Non-negative Values represents animation frames that are 
    // facing rightwards. Negative values present animation frames 
    // facing leftwards.
    signed char frame, health;
    
    // Every actor bears its own timer which ticks down every frame.
    // Logic dictating how sprites animate rely on this timer.
    unsigned char timer;
} sActor;

// Mobs can collide with tiles that have identifiers that are 
// multiples of four.
#define SOLID_TILE_PERIOD 4
typedef char TILE;
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
    
    // The game stores the width and height of the level as amounts 
    // of pixels.
    unsigned short w, h;
    
    sCoord spawn;
} sLevel;

#define MAX_ACTORS 256
#define GRAVITY 2
#define MOLD_NULL 0xFF
typedef struct {
    struct {
        union {
            sActor player;
            sActor actor[MAX_ACTORS];
        } actorData;
        unsigned short actors;
    };
} sCast;
typedef struct {
    sCast cast;
    sMoldDirectory md;
    sLevel level;
} sScene;

#define KEYS 7
typedef struct {
    unsigned char holdDur;
} sKey;
typedef struct {
    sKey right, up, left, down, run, jump, slide;
} sInput;
typedef struct {
    sScene scene;
    sInput input;
} sContext;

int initContext(sScene *s);
int freeLevelData(void);
void updateContext(sContext *c);

sActor updatePlayer(sContext *c);
int updateNpc(sScene *s, sActor *ref);

#define _HEADER_LOGIC
#endif