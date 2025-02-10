#include <limits.h>

// XXX: Remove stdio once debugging is over.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "logic.h"

// Initialise everything but the mold data of the context of the game.
void startContext(sContext *c) {
    sSprite player;
    unsigned long i;
    TILE *levelData;
    
    // Set all bytes in the mold data to the representation of the 
    // null mold. This initialisation applies to all sprites except 
    // for the player. This setup causes only the player to appear and 
    // no other sprite. The other sprites are null sprites at this 
    // initial point.
    memset(&c->scene.actorData.player + 1, MOLD_NULL, 
        sizeof c->scene.actorData - sizeof c->scene.actorData.player);
    
    // XXX: Add mold data for player
    // XXX: Match spawn coordinates for the player with the ones in 
    // the actual level. Loading the level would provide this 
    // information.
    player.pos.x = 0;
    player.pos.y = 32;
    player.moldId = 0; // XXX: Assume that the player uses a mold id 
                       // of zero.
    player.vel.x = 0;
    player.vel.y = 0;
    player.vel.subX = 0;
    player.animFrame = 0;
    
    // XXX: Increase player health.
    player.health = 1;
    
    c->scene.actorData.player = player;
    
    // XXX: Actually load a level instead of hard-coding values.
    c->scene.level.w = 200;
    c->scene.level.h = 32;
    c->scene.level.data = levelData = calloc(sizeof*c->scene.level.data 
        * c->scene.level.w * c->scene.level.h, sizeof*c->scene.level.data);
    if (c->scene.level.data == NULL) {
        return;
        
    }
    // XXX: Load the first level from files
    // XXX: Consider loading in the graphics of every tile?
    for (i = 0; i < c->scene.level.w; ++i) {
        levelData[i * c->scene.level.h] = 1;
    }
    
    // Zero out all player input states. This configuration prevents 
    // guarantees that the game does not recognize any initial inputs.
    memset(&c->input, 0x00, sizeof c->input);
    
    return;
}

typedef struct {
    char hittingCeil, hittingFloor, homingFloor;
} sCollision;

static sSprite updatePlayer(sContext const *c);
static sCollision collisionOf(sSprite const *s, sContext const *c);
#define MAX_VALUE_OF_SIGNED(A) ~~( (1 << (CHAR_BIT*sizeof(A)-1)) - 1 )
#define ABS(A) ~~((A)>0?(A):-(A))

// A result that overflows the amount of bits in signed values is 
// undefined behaviour. The dynamics simulator detects whether a mob 
// will cause this overflow after it accelerates.
#define SUBVEL_WILL_OVERFLOW(SUBVEL, ACCEL) !!(ABS(SUBVEL) \
    > MAX_VALUE_OF_SIGNED(SUBVEL)-(ACCEL))

#define MOB_GRAVITY 1
#define MOB_MAX_SPEED_Y 21
void updateContext(sContext *c) {
    c->scene.actorData.player = updatePlayer(c);
    
    return;
}

#define PLAYER_WALK_COEF(VEL) ~~(1*(VEL)/ 2)
#define PLAYER_FRICTION 10
#define PLAYER_JUMP_HOLD_FRAMES 18
#define PLAYER_JUMP_VEL 7

static sSprite updatePlayer(sContext const *c) {
    sSprite p = c->scene.actorData.player;
    sMold const mold = c->scene.md.data[p.moldId];
    sCollision const col = collisionOf(&c->scene.actorData.player, c);
    sCoord nextPos = p.pos;
    signed char const maxSpeed = (signed char) (c->input.key.run.holdDur ?
        mold.maxSpeed : PLAYER_WALK_COEF(mold.maxSpeed));
    
    if (SUBVEL_WILL_OVERFLOW(p.vel.subX, mold.subAccel)) {
        p.vel.subX > 0 ? p.vel.x++ : p.vel.x--;
        p.vel.subX = 0;
        
    }
    
    if (col.hitCeil) {
        if (p.vel.x != 0) {
            p.vel.subX = (signed char)(p.vel.x>0 ? p.vel.subX-PLAYER_FRICTION 
                : p.vel.subX+PLAYER_FRICTION);
            
        }
        p.vel.y = 0;
        nextPos.y = (unsigned short)( ((nextPos.y+TILE_PELS-1)/TILE_PELS) 
            * TILE_PELS);
        
    } else {
        p.vel.y = (signed char)(p.vel.y-MOB_GRAVITY);
        if (p.vel.y < -MOB_MAX_SPEED_Y) {
            p.vel.y = -MOB_MAX_SPEED_Y;
            
        }
        
    }
    
    if (p.vel.x < -maxSpeed) {
        p.vel.x = (signed char) -maxSpeed;
        
    } else if (p.vel.x > maxSpeed) {
        p.vel.x = (signed char) maxSpeed;
        
    }
    
    // The dynamics simulator must apply effects of jumping after 
    // those of collision.
    if (c->input.key.jump.holdDur 
            && c->input.key.jump.holdDur < PLAYER_JUMP_HOLD_FRAMES) {
        p.vel.y = PLAYER_JUMP_VEL;
        
    }
    
    if (c->input.key.left.holdDur) {
        p.vel.subX = (signed char)(p.vel.subX - mold.subAccel);
        
    }
    if (c->input.key.right.holdDur) {
        p.vel.subX = (signed char)(p.vel.subX + mold.subAccel);
        
    }
    
    nextPos.x = (unsigned short)(nextPos.x + p.vel.x);
    nextPos.y = (unsigned short)(nextPos.y + p.vel.y);
    p.pos = nextPos;
    
    return p;
}

static sCollision collisionOf(sSprite const *s, sContext const *c) {
    sMold const m = c->scene.md.data[s->moldId];
    unsigned int const h = c->scene.level.h;
    // XXX: Consider the case where the sprite reaches y positon 0.
    // Risk of underflow.
    unsigned int const tIndexTop = h*(s->pos.x/TILE_PELS)
        + (unsigned int)(s->pos.y-1)/TILE_PELS,
        tIndexBottom = h*(s->pos.x/TILE_PELS)
        + (unsigned int)(s->pos.y + m.h - 1)/TILE_PELS;
    sCollision col = {
        c->scene.level.data[tIndexTop] > 0,
        c->scene.level.data[tIndexBottom] > 0
    };
    return col;
}