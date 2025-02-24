#include <limits.h>

// XXX: Remove stdio once debugging is over.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "logic.h"

// Initialise everything but the mold data of the context of the game.
void startContext(sContext *c) {
    sActor player;
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
    player.frame = 0;
    
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

static sActor updatePlayer(sContext const *c);
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
#define PLAYER_FRICTION 26
#define PLAYER_JUMP_HOLD_FRAMES 18
#define PLAYER_SLIDE_HOLD_FRAMES 32
#define PLAYER_JUMP_VEL 7

#define POS_TO_TILE_INDEX(X, Y, H) ~~((H)*((X)/TILE_PELS) + (Y)/TILE_PELS)
static sActor updatePlayer(sContext const *c) {
    sActor p = c->scene.actorData.player;
    sMold const mold = c->scene.md.data[p.moldId];
    sLevel const level = c->scene.level;
    sCoord const prev = p.pos;
    signed short maxSpeed;
    struct {
        struct {
            char left, right;
        } floor, above, bottom;
    } hitting;
    
    // Update the position of the player, whether the player will 
    // collide or not. The logic following these updates will rely on 
    // these new coordinates to make decisions.
    p.pos.x = (unsigned short)(p.pos.x + (p.vel.subX > 0 ? 
        p.vel.subX+255 : p.vel.subX-255)/ 256);
    p.pos.y = (unsigned short)(p.pos.y + p.vel.y);
    
    // Prevent players from underflowing their current position. Such 
    // underflows can cause the collision detector to calculate 
    // out-of-bounds tilemap indices.
    if (p.vel.subX < 0 && prev.x < p.pos.x) {
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
    // the level? (right-most side and highest position)
    
    if (c->input.run.holdDur) {
        maxSpeed = (signed short) (mold.maxSpeed<<8);
        
    } else {
        maxSpeed = (signed short) PLAYER_WALK_COEF(mold.maxSpeed<<8);
        
    }
    
    // There are two ways to cause the player to snap to the ground. 
    // One way first requires the player to bear a vertical velocity 
    // of zero. Then, the game checks whether the player is directly 
    // under a solid tile. The player snaps to the ground under these 
    // conditions. Another avenue trigger this behaviour exists that 
    // does not involve velocities. This alternative requires the 
    // pixel row below the player to touch a solid tile. Here, the 
    // player snaps if a non-solid tile is directly above the solid 
    // tile. The player must also be falling at the speed of their 
    // jump. This last condition guarantees to ground the player when 
    // they are jumping in place. Note that there is a caveat with 
    // this collision detection algorithm. Is it possible for the 
    // player to clip inside a wall and jump. This situation occurs if 
    // the player has zero vertical velocity and hugs a wall. Note 
    // that these conditions disregard whether the player is airborne 
    // or not. As such, the player can jump off a wall with careful 
    // timing.
    hitting.floor.left = level.data[POS_TO_TILE_INDEX(p.pos.x, p.pos.y-1, 
        level.h)] > 0;
    hitting.floor.right = level.data[POS_TO_TILE_INDEX(p.pos.x+mold.w-1,
        p.pos.y-1, level.h)] > 0;
    hitting.above.left = level.data[POS_TO_TILE_INDEX(p.pos.x, 
        p.pos.y+TILE_PELS-1, level.h)] > 0;
    hitting.above.right = level.data[POS_TO_TILE_INDEX(p.pos.x+mold.w-1, 
        p.pos.y+TILE_PELS-1, level.h)] > 0;
    if (((hitting.floor.left||hitting.floor.right)&&p.vel.y==0)
            || (p.vel.y <= -PLAYER_JUMP_VEL
            &&( ((hitting.floor.left&&!hitting.above.left)
            ||(hitting.floor.right&&!hitting.above.right)) ))) {
        if (c->input.jump.holdDur == 1) {
            p.vel.y = PLAYER_JUMP_VEL;
            
        } else {
            p.vel.y = 0;
            p.pos.y = (unsigned short)( ((p.pos.y + TILE_PELS-1)/TILE_PELS) 
                * TILE_PELS );
            
        }
        
        if (p.vel.subX != 0) {
            
            // Switch the animation frame's direction if the 
            // velocity and the sprite's orientation are opposite.
            if ((p.vel.subX>0) ^ (p.frame>=0)) {
                p.frame = (signed char)~p.frame;
             
            }
            
            // Sliding cancels out all friction.
            if (c->input.slide.holdDur > 0 
                    && c->input.slide.holdDur < PLAYER_SLIDE_HOLD_FRAMES) {
                signed short const slidingSpeed = (signed short)
                    (mold.maxSpeed << 8);
                
                if (c->input.left.holdDur && (p.vel.subX == -slidingSpeed
                        || c->input.slide.holdDur == 1)) {
                    p.vel.subX = (signed short) -slidingSpeed;
                    maxSpeed =  slidingSpeed;
                    
                } else if (c->input.right.holdDur 
                        && (p.vel.subX == slidingSpeed
                        || c->input.slide.holdDur == 1)) {
                    p.vel.subX = slidingSpeed;
                    maxSpeed =  slidingSpeed;
                    
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
                
            }
        }
        
    } else {
        // XXX: Implement collision for hitting ceilings.
        
        if (p.vel.y == +PLAYER_JUMP_VEL 
                && c->input.jump.holdDur > 0
                && c->input.jump.holdDur < PLAYER_JUMP_HOLD_FRAMES) {
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
    hitting.bottom.left = level.data[POS_TO_TILE_INDEX(p.pos.x, p.pos.y, 
        level.h)] > 0;
    if (hitting.bottom.left) {
        
        p.vel.subX = 0;
        p.pos.x = (unsigned short)(((p.pos.x + TILE_PELS-1)/TILE_PELS)
            *TILE_PELS);
        
    } else {
        hitting.bottom.right = level.data[POS_TO_TILE_INDEX(p.pos.x+mold.w-1, 
            p.pos.y, level.h)] > 0;
        if (hitting.bottom.right) {
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
    if (c->input.left.holdDur) {
        p.vel.subX = (signed short)(p.vel.subX - mold.subAccel);
        
    }
    if (c->input.right.holdDur) {
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
