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
    player.vel.y = 0;
    player.vel.subX = 0;
    
    // The player begins by facing right.
    player.animFrame = 0;
    
    // XXX: Increase player health.
    player.health = 1;
    
    c->scene.actorData.player = player;
    
    // XXX: Actually load a level instead of hard-coding values.
    c->scene.level.w = 200;
    c->scene.level.h = 32;
    c->scene.level.spawn = player.pos;
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
    levelData[2*c->scene.level.h] = 0;
    levelData[1 + 4*c->scene.level.h] = 1;
    levelData[2 + 4*c->scene.level.h] = 1;
    levelData[3 + 4*c->scene.level.h] = 1;
    
    // Zero out all player input states. This configuration prevents 
    // guarantees that the game does not recognize any initial inputs.
    memset(&c->input, 0x00, sizeof c->input);
    
    return;
}

typedef struct {
    char hittingCeil, hittingFloor, hittingLeft, hittingRight;
} sCollision;

static sSprite updatePlayer(sContext const *c);
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
#define PLAYER_FRICTION 16
#define PLAYER_JUMP_HOLD_FRAMES 18
#define PLAYER_SLIDE_HOLD_FRAMES 32
#define PLAYER_JUMP_VEL 7

#define POS_TO_TILE_INDEX(X, Y, H) ~~((H)*((X)/TILE_PELS) + (Y)/TILE_PELS)
static sSprite updatePlayer(sContext const *c) {
    sSprite p = c->scene.actorData.player;
    sMold const mold = c->scene.md.data[p.moldId];
    sLevel const level = c->scene.level;
    unsigned short const prevX = p.pos.x;
    struct {
        char floor, left, right;
    } hitting;
    signed short maxSpeed;
    
    // Update the position of the player, whether the player will 
    // collide or not. The logic following these updates will rely on 
    // these new coordinates to make decisions.
    p.pos.x = (unsigned short)(p.pos.x + (p.vel.subX > 0 ? 
        p.vel.subX+255 : p.vel.subX-255)/ 256);
    p.pos.y = (unsigned short)(p.pos.y + p.vel.y);
    
    // Prevent players from underflowing their current position. Such 
    // underflows can cause the collision detector to calculate 
    // out-of-bounds tilemap indices.
    if (p.vel.subX < 0 && prevX < p.pos.x) {
        p.pos.x = 0;
        p.vel.subX = 0;
        
    }
    
    // Ditto, but for the player's vertical position.
    if (p.vel.y < 0
            && p.pos.y > (unsigned short)(p.pos.y - p.vel.y)) {
        p.pos = level.spawn;
        p.vel.subX = 0;
        p.vel.y = 0;
        
    }
    
    // XXX: Reuse logic above to keep the player within the bounds of 
    // the level?
    
    hitting.floor = level.data[POS_TO_TILE_INDEX(p.pos.x, p.pos.y-1, 
        level.h)] > 0
        || level.data[POS_TO_TILE_INDEX(p.pos.x + mold.w-1,
        p.pos.y-1, level.h)] > 0;
    
    if (c->input.key.run.holdDur) {
        maxSpeed = (signed short) (mold.maxSpeed<<8);
        
    } else {
        maxSpeed = (signed short) PLAYER_WALK_COEF(mold.maxSpeed<<8);
        
    }
    
    if (hitting.floor) {
        
        if (c->input.key.jump.holdDur == 1) {
            p.vel.y = PLAYER_JUMP_VEL;
            
        } else {
            p.vel.y = 0;
            
        }
        p.pos.y = (unsigned short)( ((p.pos.y + TILE_PELS-1)/TILE_PELS) 
            * TILE_PELS );
        
        if (p.vel.subX != 0) {
            
            // Sliding cancels out all friction.
            if (c->input.key.slide.holdDur == 1
                    || (c->input.key.slide.holdDur < PLAYER_SLIDE_HOLD_FRAMES
                    && c->input.key.slide.holdDur > 0
                    &&((c->input.key.left.holdDur&&!c->input.key.right.holdDur
                    &&p.vel.subX>>8==-mold.maxSpeed)
                    || (c->input.key.right.holdDur&&!c->input.key.left.holdDur 
                    &&p.vel.subX>>8==mold.maxSpeed)))) {
                maxSpeed = (signed short) (mold.maxSpeed<<8);
                
                if (c->input.key.right.holdDur&&!c->input.key.left.holdDur) {
                    p.vel.subX = (signed short) maxSpeed;
                    
                } else if (c->input.key.left.holdDur) {
                    p.vel.subX = (signed short) -maxSpeed;
                
                }
                
            } else {
                int const goingRight = p.vel.subX > 0;
                
                p.vel.subX = (signed short)(goingRight ? 
                    p.vel.subX-PLAYER_FRICTION : p.vel.subX+PLAYER_FRICTION);
                
                // Reset the player velocity if friction causes a 
                // change in orientation.
                if (goingRight ^ (p.vel.subX > 0)) {
                    p.vel.subX = 0;
                    
                }
                
                // Switch the animation frame's direction if the 
                // velocity and the sprite's orientation are opposite.
                // XXX: Add logic for mirroring sprites.
                // if (goingRight ^ (p.animFrame>=0)) {
                    // p.animFrame = (signed char)~p.animFrame;
                    
                // }
                
            }
        }
        
    } else {
        if (p.vel.y == +PLAYER_JUMP_VEL 
                && c->input.key.jump.holdDur > 0
                && c->input.key.jump.holdDur < PLAYER_JUMP_HOLD_FRAMES) {
            p.vel.y = PLAYER_JUMP_VEL;
            
        } else {
            p.vel.y = (signed char)(p.vel.y-MOB_GRAVITY);
            if (p.vel.y < -MOB_MAX_SPEED_Y) {
                p.vel.y = -MOB_MAX_SPEED_Y;
                
            }
            
        }
        
    }
    
    // Evaluate left and right collisions after potentially snapping 
    // the player to the ground.
    hitting.left = level.data[POS_TO_TILE_INDEX(p.pos.x, p.pos.y, 
        level.h)] > 0;
    if (hitting.left) {
        p.vel.subX = 0;
        p.pos.x = (unsigned short)(((p.pos.x + TILE_PELS-1)/TILE_PELS)
            *TILE_PELS);
        
    } else {
        hitting.right = level.data[POS_TO_TILE_INDEX(p.pos.x + mold.w-1, 
            p.pos.y, level.h)] > 0;
        if (hitting.right) {
            p.vel.subX = 0;
            
            // Set the coordinate of the player's bottom left pixel
            // to a tile's coordinate.
            p.pos.x = (unsigned short)((p.pos.x/TILE_PELS)*TILE_PELS);
            
            // Shift the player such that the bottom right pixel 
            // is immediately before the tile.
            p.pos.x = (unsigned short)(p.pos.x + (TILE_PELS-mold.w));
            
        }
    }
    
    // Change the sub-velocity if necessary to calculate the current 
    // horizontal velocity.
    if (c->input.key.left.holdDur) {
        p.vel.subX = (signed short)(p.vel.subX - mold.subAccel);
        
    }
    if (c->input.key.right.holdDur) {
        p.vel.subX = (signed short)(p.vel.subX + mold.subAccel);
        
    }
    
    // The dynamics simulator must apply speed caps at the end of the 
    // motion update.
    if (p.vel.subX > maxSpeed) {
        p.vel.subX = maxSpeed;
        
    } else if (p.vel.subX < -maxSpeed) {
        p.vel.subX = (signed short) -maxSpeed;
        
    }
    
    return p;
}
