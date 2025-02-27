#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "logic.h"

static int loadLevel(sLevel *dst);
static int initActorData(sCast *c);

int initContext(sContext *c) {
    sActor player;
    
    // Set all bytes in the mold data to the representation of the 
    // null mold. This initialisation applies to all sprites except 
    // for the player. This setup causes only the player to appear and 
    // no other sprite. The other sprites are null sprites at this 
    // initial point.
    memset(&c->scene.cast.actorData.player + 1, MOLD_NULL, 
        sizeof c->scene.cast.actorData 
        - sizeof c->scene.cast.actorData.player);
    
    if (loadLevel(&c->scene.level) || initActorData(&c->scene.cast)) {
        return 1;
        
    }
    
    player.moldId = 0;
    player.vel.y = 0;
    player.vel.subX = 0;
    
    // The player begins by facing right.
    player.frame = 0;
    player.timer = 0;
    
    // XXX: Increase player health.
    player.health = 1;
    
    player.pos = c->scene.level.spawn;
    
    c->scene.cast.actorData.player = player;
    
    // Zero out all player input states. This configuration prevents 
    // guarantees that the game does not recognize any initial inputs.
    memset(&c->input, 0x00, sizeof c->input);
    
    return 0;
}

// Store a non-constant version of the pointer to the tilemap to free 
// it eventually.
static TILE *tileData;

int freeLevelData(void) {
    if (tileData != NULL) {
        free(tileData);
        return 0;
        
    }
    return 1;
}

#include <limits.h>

#include "global_dict.h"

#define LEVEL_HEADER_BYTES 6U
#define LEVEL_HEADER_COORD_BITS 12U
#define LEVEL_HEADER_COORDS 4U
static int loadLevel(sLevel *dst) {
    FILE *f;
    char buffer[64], code;
    
    f = fopen(DIR_LEVEL_TILEMAP, "rb");
    if (f == NULL) {
        return 1;
        
    }
    
    do {
        unsigned short *coordAsTiles[LEVEL_HEADER_COORDS];
        unsigned long i, tiles, tileIndex;
        unsigned int bitsLeft, coordIndex, ioBytes;
        unsigned short value;
        
        if ((ioBytes = (unsigned int)
                fread(buffer, sizeof*buffer, LEVEL_HEADER_BYTES, f))
                != LEVEL_HEADER_BYTES) {
            code = 1;
            break;
            
        }
        
        // Set up the memory addresses of the level attributes.
        coordAsTiles[0] = &dst->w;
        coordAsTiles[1] = &dst->h;
        coordAsTiles[2] = &dst->spawn.x;
        coordAsTiles[3] = &dst->spawn.y;
        for (i = 0, bitsLeft = LEVEL_HEADER_COORD_BITS, coordIndex = 0, 
                value = 0;
                coordIndex < ARRAY_ELEMENTS(coordAsTiles); 
                ++i) {
            unsigned char const byte = (unsigned char)buffer[i];
            
            // Assume that the sequence of bits of a value can straddle 
            // over two bytes.
            if (bitsLeft >= CHAR_BIT) {
                bitsLeft -= CHAR_BIT;
                value = (unsigned short)(value + buffer[i]);
                value = (unsigned short) (value << bitsLeft);
                
            } else {
                unsigned int const shift = (unsigned int)(CHAR_BIT-bitsLeft),
                    remainder = (unsigned int) (byte >> shift);
                value = (unsigned short)(value + remainder);
                *coordAsTiles[coordIndex++] = value;
                value = (unsigned short) (byte - (remainder<<shift));
                bitsLeft = LEVEL_HEADER_COORD_BITS - shift;
                
            }
        }
        
        // Scale the player spawn coordinates according to the size 
        // of tiles.
        dst->spawn.x = (unsigned short)(dst->spawn.x * TILE_PELS);
        dst->spawn.y = (unsigned short)(dst->spawn.y * TILE_PELS);
        
        tiles = (unsigned long)dst->w * (unsigned long)dst->h;
        tileData = malloc((size_t)tiles * sizeof*tileData);
        if (tileData == NULL) {
            code = 1;
            break;
            
        }
        
        tileIndex = 0;
        do {
            while (i < ioBytes) {
                char const byte = buffer[i++];
                tileData[tileIndex++] = (TILE) ((byte&0xF0) >> 4);
                tileData[tileIndex++] = (TILE) (byte&0x0F);
                
            }
            i = 0;
            
            ioBytes = (unsigned int)fread(buffer, sizeof*buffer, 
                ARRAY_ELEMENTS(buffer), f);
        } while (ioBytes != 0);
        
        dst->data = tileData;
        code = 0;
    } while (0);
    
    fclose(f);
    
    return code;
}

static int initActorData(sCast *c) {
    FILE *f;
    char buffer[64];
    unsigned int ioBytes, actorIndex;
    struct {
        char keyword[4], foundNumber, keywordEnd;
        unsigned short number;
        enum {
            SEARCH_ENTRY,
            SEARCH_ENEMY,
            SEARCH_COORD_X,
            SEARCH_COORD_Y
        } search;
    } state;
    
    f = fopen(DIR_LEVEL_GEN, "rb");
    if (f == NULL) {
        return 1;
        
    }
    
    memset(&state.keyword, 0x00, sizeof state.keyword);
    state.search = SEARCH_ENTRY;
    state.keywordEnd = 1;
    state.foundNumber = 0;
    actorIndex = 0;
    while ((ioBytes = (unsigned int)
            fread(buffer, sizeof*buffer, sizeof buffer, f)) != 0) {
        unsigned int byteIndex;
        
        for (byteIndex = 0; byteIndex < ioBytes; ) {
            char const byte = buffer[byteIndex++];
            struct {
                char const enemy[4];
                char const coord[2];
            } vocab = {
                "teki",
                "xy"
            };
            
            switch (byte) {
                case ';': {
                    if (++actorIndex >= ARRAY_ELEMENTS(c->actorData.actor)) {
                        break;
                        
                    }
                    
                }
                // Fall-through
                case '\n':
                case '\t': 
                case '\r': 
                case ' ': {
                    if (state.foundNumber) {
                        sActor (*dst)[MAX_ACTORS] = &c->actorData.actor;
                        
                        if (state.search == SEARCH_ENTRY) {
                            break;
                            
                        }
                        
                        switch (state.search) {
                            case SEARCH_ENEMY: {
                                (*dst)[actorIndex].moldId = (unsigned char) 
                                    state.number;
                                state.search = SEARCH_ENTRY;
                                break;
                                
                            }
                            
                            case SEARCH_COORD_X: {
                                (*dst)[actorIndex].pos.x = (unsigned short) 
                                    state.number;
                                state.search = SEARCH_COORD_Y;
                                break;
                                
                            }
                            
                            case SEARCH_COORD_Y: {
                                (*dst)[actorIndex].pos.y = (unsigned short) 
                                    state.number;
                                state.search = SEARCH_ENTRY;
                                break;
                                
                            }
                            
                            default:
                        }
                        state.foundNumber = 0;
                        state.number = 0U;
                        
                    } else if (!state.keywordEnd) {
                        state.keywordEnd = 1;
                        
                    }
                    if (byte == ';') {
                        state.search = SEARCH_ENTRY;
                        
                    }
                    continue;
                    
                }
                
                case ':': {
                    state.keywordEnd = 1;
                    
                }
                // Fall-through
                case 't': 
                case 'e': 
                case 'k':
                case 'i':
                case 'x': 
                case 'y': {
                    if (state.foundNumber) {
                        break;
                        
                    }
                    
                    if (state.keywordEnd) {
                        if (memcmp(state.keyword, vocab.enemy, 
                                sizeof vocab.enemy) == 0) {
                            state.search = SEARCH_ENEMY;
                            
                        }
                        if (memcmp(&state.keyword[2], vocab.coord,
                                sizeof vocab.coord) == 0) {
                            state.search = SEARCH_COORD_X;
                            
                        }
                        
                    }
                    
                    state.keyword[0] = state.keyword[1];
                    state.keyword[1] = state.keyword[2];
                    state.keyword[2] = state.keyword[3];
                    state.keyword[3] = byte;
                    state.number = 0U;
                    state.keywordEnd = 0;
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
                    state.number = (unsigned short)(state.number * 10U);
                    state.number = (unsigned short)(state.number 
                        + (byte-'0'));
                    state.foundNumber = 1;
                    continue;
                    
                }
                
                default:
            }
            
            fclose(f);
            return 1;
            
        }
    }
    
    fclose(f);
    return (state.search != SEARCH_ENTRY) | (state.foundNumber);
}

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
    c->scene.cast.actorData.player = updatePlayer(c);
    
    return;
}

#define PLAYER_WALK_COEF(VEL) ~~(1*(VEL)/ 2)
#define PLAYER_FRICTION 26
#define PLAYER_JUMP_HOLD_FRAMES 18
#define PLAYER_SLIDE_HOLD_FRAMES 32
#define PLAYER_JUMP_VEL 7

enum {
    ANIM_PLAYER_WALK_NEUTRAL,
    ANIM_PLAYER_WALK_LEFT,
    ANIM_PLAYER_WALK_RIGHT,
    ANIM_PLAYER_RUN_NEUTRAL,
    ANIM_PLAYER_RUN_LEFT,
    ANIM_PLAYER_RUN_RIGHT,
    ANIM_PLAYER_DASH,
    ANIM_PLAYER_SLIDE,
    ANIM_PLAYER_GETUP,
    ANIM_PLAYER_HURT,
    ANIM_PLAYER_DEAD,
    ANIM_PLAYER_JUMP,
    ANIM_PLAYER_CROUCH
};

#define ANIM_DASH_SLIDE_TRANSITION 2U
#define ANIM_SLIDE_GETUP_TRANSITION 3U
#define ANIM_GAIT_TRANSITION_PERIOD 8U

#define POS_TO_TILE_INDEX(X, Y, H) ~~((H)*((X)/TILE_PELS) + (Y)/TILE_PELS)
static sActor updatePlayer(sContext const *c) {
    sActor p = c->scene.cast.actorData.player;
    sMold const mold = c->scene.md.data[p.moldId];
    sLevel const level = c->scene.level;
    sCoord const prev = p.pos;
    signed short maxSpeed;
    struct {
        struct {
            char left, right;
        } floor, step, bottom, ceil;
    } hitting;
    unsigned char absFrame;
    
    // Update the vertical position of the player, whether the player 
    // will collide or not. The logic following these updates will 
    // rely on these new coordinates to make decisions.
    p.pos.y = (unsigned short)(p.pos.y + p.vel.y);
    
    // Prevent players from underflowing their current position. Such 
    // underflows can cause the collision detector to calculate 
    // out-of-bounds tilemap indices.
    if (p.vel.y < 0
            && p.pos.y > (unsigned short)(p.pos.y - p.vel.y)) {
        p.pos = level.spawn;
        p.vel.subX = 0;
        p.vel.y = 0;
        
    }
    
    if (c->input.run.holdDur) {
        maxSpeed = (signed short) (mold.maxSpeed<<8);
        
    } else {
        maxSpeed = (signed short) PLAYER_WALK_COEF(mold.maxSpeed<<8);
        
    }
    
    // This function deduces the player's orientation from its `frame` 
    // attribute. The function must perform manipulations on a copy of 
    // this value.
    if (p.frame < 0) {
        absFrame = (unsigned char) ~p.frame;
        
    } else {
        absFrame = (unsigned char) p.frame;
    
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
    // they are jumping in place.
    hitting.floor.left = level.data[POS_TO_TILE_INDEX(p.pos.x, p.pos.y-1, 
        level.h)] % SOLID_TILE_PERIOD == 0;
    hitting.floor.right = level.data[POS_TO_TILE_INDEX(p.pos.x+mold.w-1,
        p.pos.y-1, level.h)] % SOLID_TILE_PERIOD == 0;
    hitting.step.left = level.data[POS_TO_TILE_INDEX(p.pos.x, 
        p.pos.y+TILE_PELS-1, level.h)] % SOLID_TILE_PERIOD == 0;
    hitting.step.right = level.data[POS_TO_TILE_INDEX(p.pos.x+mold.w-1, 
        p.pos.y+TILE_PELS-1, level.h)] % SOLID_TILE_PERIOD == 0;
    if (((hitting.floor.left||hitting.floor.right)&&p.vel.y==0)
            || (p.vel.y <= -PLAYER_JUMP_VEL
            &&( ((hitting.floor.left&&!hitting.step.left)
            ||(hitting.floor.right&&!hitting.step.right)) ))) {
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
                    && c->input.slide.holdDur < PLAYER_SLIDE_HOLD_FRAMES
                    && !c->input.down.holdDur) {
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
                
                if (absFrame != ANIM_PLAYER_DASH 
                        && absFrame != ANIM_PLAYER_SLIDE) {
                    absFrame = ANIM_PLAYER_DASH;
                    p.timer = 1U;
                    
                } else {
                    if (p.timer < ANIM_DASH_SLIDE_TRANSITION) {
                        ++p.timer;
                    
                    } else {
                        absFrame = ANIM_PLAYER_SLIDE;
                        
                    }
                }
                
            } else {
                int const goingRight = p.vel.subX > 0;
                unsigned short const absVelSubX = (unsigned short)
                    ((unsigned int) ABS(p.vel.subX) >> 8U);
                unsigned char const changePeriod = (unsigned char)
                    (ANIM_GAIT_TRANSITION_PERIOD - absVelSubX);
                // High horizontal velocities may cause modulo by zero.
                
                p.vel.subX = (signed short)(goingRight ? 
                    p.vel.subX-PLAYER_FRICTION : p.vel.subX+PLAYER_FRICTION);
                
                // Reset the player velocity if friction causes a 
                // change in orientation.
                if (goingRight ^ (p.vel.subX > 0)) {
                    p.vel.subX = 0;
                    absFrame = ANIM_PLAYER_RUN_NEUTRAL;
                    
                }
                
                if (absFrame == ANIM_PLAYER_SLIDE
                        || absFrame == ANIM_PLAYER_GETUP) {
                    if (p.timer - ANIM_DASH_SLIDE_TRANSITION 
                            < ANIM_SLIDE_GETUP_TRANSITION) {
                        absFrame = ANIM_PLAYER_GETUP;
                        
                    } else {
                        absFrame = ANIM_PLAYER_RUN_NEUTRAL;
                        
                    }
                    
                } else if (((p.vel.subX > 0&&c->input.left.holdDur)
                        ||(p.vel.subX < 0&&c->input.right.holdDur))
                        && c->input.run.holdDur) {
                    absFrame = ANIM_PLAYER_DASH;
                    
                } else if (p.timer%changePeriod == 0) {
                    struct {
                        unsigned char left, right, neutral;
                    } frame;
                    
                    if (c->input.run.holdDur) {
                        frame.left = ANIM_PLAYER_RUN_LEFT;
                        frame.right = ANIM_PLAYER_RUN_RIGHT;
                        frame.neutral = ANIM_PLAYER_RUN_NEUTRAL;
                        
                    } else {
                        frame.left = ANIM_PLAYER_WALK_LEFT;
                        frame.right = ANIM_PLAYER_WALK_RIGHT;
                        frame.neutral = ANIM_PLAYER_WALK_NEUTRAL;
                        
                    }
                    
                    if (p.timer % (2U*changePeriod) == 0) {
                        if (p.timer % (4U*changePeriod) == 0) {
                            absFrame = frame.left;
                            
                        } else {
                            absFrame = frame.right;
                            
                        }
                        
                    } else {
                        absFrame = frame.neutral;
                        
                    }
                    
                }
                ++p.timer;
                
            }
        } else {
            absFrame = ANIM_PLAYER_WALK_NEUTRAL;
            
        }
        
    } else {
        hitting.ceil.left = level.data[POS_TO_TILE_INDEX(p.pos.x, 
            p.pos.y + mold.h-1, level.h)] % SOLID_TILE_PERIOD == 0;
        hitting.ceil.right = level.data[POS_TO_TILE_INDEX(p.pos.x + mold.w-1, 
            p.pos.y + mold.h-1, level.h)] % SOLID_TILE_PERIOD == 0;
        
        if (hitting.ceil.left || hitting.ceil.right
                || (p.pos.y + mold.h-1) > TILE_PELS*level.h) {
            p.pos.y = (unsigned short)((p.pos.y/TILE_PELS)*TILE_PELS
                - (mold.h-TILE_PELS));
            p.vel.y = 0;
            
        } else {
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
        
        absFrame = ANIM_PLAYER_JUMP;
        
    }
    
    // Update the player's horizontal position after updating the 
    // vertical position regardless of resulting collisions. Order of 
    // position updates snaps the player to the ground before updating 
    // horizontal positions. The collision logic relies on detecting 
    // whether the player is inside solid tiles or not. The logic then 
    // alters the player's motion accordingly.
    p.pos.x = (unsigned short)(p.pos.x + (p.vel.subX > 0 ? 
        p.vel.subX+255 : p.vel.subX-255)/ 256);
    
    // Prevent the player from overflowing their horizontal position.
    if (p.vel.subX < 0 && prev.x < p.pos.x) {
        p.pos.x = 0;
        p.vel.subX = 0;
        
    }
    
    // Prevent players from going beyond the boundaries of the level.
    if (p.pos.x + mold.w > TILE_PELS*level.w 
            || (p.vel.subX > 0 && prev.x > p.pos.x)) {
        p.pos.x = (unsigned short)(level.w * TILE_PELS
            - mold.w);
        p.vel.subX = 0;
        
    }
    
    // Evaluate left and right collisions after potentially snapping 
    // the player to the ground.
    hitting.bottom.left = level.data[POS_TO_TILE_INDEX(p.pos.x, p.pos.y, 
        level.h)] % SOLID_TILE_PERIOD == 0;
    if (hitting.bottom.left) {
        p.vel.subX = 0;
        p.pos.x = (unsigned short)(((p.pos.x + TILE_PELS-1)/TILE_PELS)
            *TILE_PELS);
        
    } else {
        hitting.bottom.right = level.data[POS_TO_TILE_INDEX(p.pos.x+mold.w-1, 
            p.pos.y, level.h)] % SOLID_TILE_PERIOD == 0;
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
    
    // Do not animate the player when the motion update nullifies 
    // horizontal velocity.
    if (p.vel.subX == 0 && p.vel.y == 0) {
        absFrame = ANIM_PLAYER_WALK_NEUTRAL;
        
    }
    
    if (c->input.down.holdDur) {
        absFrame = ANIM_PLAYER_CROUCH;
        
    } else {
        // Change the sub-velocity if necessary to calculate the current 
        // horizontal velocity.
        if (c->input.left.holdDur) {
            p.vel.subX = (signed short)(p.vel.subX - mold.subAccel);
            
        }
        if (c->input.right.holdDur) {
            p.vel.subX = (signed short)(p.vel.subX + mold.subAccel);
            
        }
        
    }
    
    // The dynamics simulator must apply speed caps at the end of the 
    // motion update.
    if (p.vel.subX > maxSpeed) {
        p.vel.subX = maxSpeed;
        
    } else if (p.vel.subX < -maxSpeed) {
        p.vel.subX = (signed short) -maxSpeed;
        
    }
    
    // Format the player's animation frame according to the player's 
    // orientation.
    if (p.frame < 0) {
        p.frame = (signed char) ~absFrame;
        
    } else {
        p.frame = (signed char) absFrame;
        
    }
    
    return p;
}
