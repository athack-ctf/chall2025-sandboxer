#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "logic.h"

enum {
    MOLDID_PLAYER,
    MOLDID_HUNTER,
    MOLDID_NINGEN = 2
};

static int loadLevel(sLevel *dst);
static int initActorData(sCast *c, sCoord defaultPos);

int initContext(sScene *s) {
    if (loadLevel(&s->level)) {
        return 1;
        
    }
    
    if (initActorData(&s->cast, s->level.spawn)) {
        return 1;
        
    }
    
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

#define ACTOR_DEFAULT_HEALTH 1

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
        unsigned long byteIndex, tiles, tileIndex;
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
        for (byteIndex = 0, bitsLeft = LEVEL_HEADER_COORD_BITS, 
                coordIndex = 0, value = 0;
                coordIndex < ARRAY_ELEMENTS(coordAsTiles); 
                ++byteIndex) {
            unsigned char const byte = (unsigned char) buffer[byteIndex];
            
            // Assume that the sequence of bits of a value can straddle 
            // over two bytes.
            if (bitsLeft >= CHAR_BIT) {
                bitsLeft -= CHAR_BIT;
                value = (unsigned short) (value+byte);
                
            } else {
                unsigned short const remainder = (unsigned short)
                    (byte>>(CHAR_BIT-bitsLeft));
                value = (unsigned short) (value+remainder);
                
                *coordAsTiles[coordIndex++] = value;
                value = (unsigned short) (byte - (remainder<<bitsLeft));
                bitsLeft = (unsigned int)
                    (LEVEL_HEADER_COORD_BITS - (CHAR_BIT-bitsLeft));
                
            }
            
            value = (unsigned short) (value << bitsLeft);
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
            while (byteIndex < ioBytes) {
                char const byte = buffer[byteIndex++];
                tileData[tileIndex++] = (TILE) ((byte&0xF0) >> 4);
                tileData[tileIndex++] = (TILE) (byte&0x0F);
                
            }
            byteIndex = 0;
            
            ioBytes = (unsigned int)fread(buffer, sizeof*buffer, 
                ARRAY_ELEMENTS(buffer), f);
        } while (ioBytes != 0);
        
        dst->data = tileData;
        code = 0;
    } while (0);
    
    fclose(f);
    
    return code;
}

// The game remembers the original configuration of actors in the 
// `sCast` struct. The game resets the scene to this actor 
// configuration when the player dies.
static sCast initialCast;

static int initActorData(sCast *c, sCoord defaultPos) {
    FILE *f;
    char buffer[64];
    unsigned int ioBytes, actorIndex;
    struct {
        char keyword[4];
        unsigned short number;
        enum {
            PARSE_SEPARATOR,
            PARSE_KEYWORD,
            PARSE_NUMBER,
            PARSE_WHITESPACE
        } parse;
        enum {
            SEARCH_PREPARE,
            SEARCH_ENTRY,
            SEARCH_ENEMY,
            SEARCH_COORD_X,
            SEARCH_COORD_Y,
            SEARCH_HEALTH
        } search;
    } state;
    
    f = fopen(DIR_LEVEL_GEN, "rb");
    if (f == NULL) {
        return 1;
        
    }
    
    memset(&state.keyword, 0x00, sizeof state.keyword);
    state.search = SEARCH_PREPARE;
    state.number = 0U;
    state.parse = PARSE_WHITESPACE;
    actorIndex = 0U;
    while ((ioBytes = (unsigned int)
            fread(buffer, sizeof*buffer, sizeof buffer, f)) != 0) {
        unsigned int byteIndex = 0;
        
        while (byteIndex < ioBytes) {
            struct {
                char const enemy[4];
                char const coord[2];
                char const health[2];
            } const vocab = {
                "teki",
                "xy",
                "hp"
            };
            char byte;
            
            if (state.search == SEARCH_PREPARE) {
                
                // Default per-enemy attributes.
                c->actorData.actor[actorIndex].pos = defaultPos;
                c->actorData.actor[actorIndex].vel.subX = 0;
                c->actorData.actor[actorIndex].vel.y = 0;
                c->actorData.actor[actorIndex].moldId = MOLD_NULL;
                c->actorData.actor[actorIndex].health = ACTOR_DEFAULT_HEALTH;
                
                // An actor faces rightwards by default.
                c->actorData.actor[actorIndex].frame = 0;
                
                // All actors begin their respective timers at zero.
                c->actorData.actor[actorIndex].timer = 0;
                
                state.search = SEARCH_ENTRY;
                
            }
            byte = buffer[byteIndex++];
            
            switch (byte) {
                case ';': {
                    if (actorIndex >= ARRAY_ELEMENTS(c->actorData.actor)) {
                        break;
                        
                    }
                    
                }
                // Fall-through
                case '\n':
                case '\t': 
                case '\r': 
                case ' ': {
                    if (state.parse == PARSE_NUMBER) {
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
                            
                            case SEARCH_HEALTH: {
                                (*dst)[actorIndex].health = (signed char) 
                                    state.number;
                                state.search = SEARCH_ENTRY;
                                break;
                                
                            }
                            
                            default:
                        }
                        state.parse = PARSE_WHITESPACE;
                        state.number = 0U;
                        
                    } else if (state.parse != PARSE_WHITESPACE 
                            && state.parse != PARSE_SEPARATOR) {
                        state.parse = PARSE_SEPARATOR;
                        
                    }
                    if (byte == ';') {
                        state.search = SEARCH_PREPARE;
                        state.parse = PARSE_WHITESPACE;
                        ++actorIndex;
                        
                    }
                    continue;
                    
                }
                
                case ':': {
                    state.parse = PARSE_SEPARATOR;
                    
                }
                // Fall-through
                case 't': 
                case 'e': 
                case 'k':
                case 'i':
                case 'x': 
                case 'y':
                case 'h':
                case 'p':{
                    if (state.parse == PARSE_NUMBER) {
                        break;
                        
                    }
                    
                    if (state.parse == PARSE_SEPARATOR) {
                        char const *end = 
                            &state.keyword[sizeof state.keyword];
                        
                        if (memcmp(end - sizeof vocab.enemy, vocab.enemy, 
                                sizeof vocab.enemy) == 0) {
                            state.search = SEARCH_ENEMY;
                            
                        } else if (memcmp(end - sizeof vocab.coord, 
                                vocab.coord, sizeof vocab.coord) == 0) {
                            state.search = SEARCH_COORD_X;
                            
                        } else if (memcmp(end - sizeof vocab.health, 
                                vocab.health, sizeof vocab.health) == 0) {
                            state.search = SEARCH_HEALTH;
                            
                        } else {
                            
                            // The parser did not recognize a keyword.
                            break;
                            
                        }
                        
                        
                        
                    }
                    
                    state.keyword[0] = state.keyword[1];
                    state.keyword[1] = state.keyword[2];
                    state.keyword[2] = state.keyword[3];
                    state.keyword[3] = byte;
                    state.number = 0U;
                    state.parse = PARSE_KEYWORD;
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
                    if (state.search == SEARCH_ENTRY) {
                        
                        // A number must follow a keyword. Otherise, 
                        // syntax is illegal.
                        break;
                        
                    }
                    
                    state.number = (unsigned short)(state.number * 10U);
                    state.number = (unsigned short)(state.number 
                        + (byte-'0'));
                    state.parse = PARSE_NUMBER;
                    continue;
                    
                }
                
                default:
            }
            
            fclose(f);
            return 1;
            
        }
    }
    
    c->actors = (unsigned short) actorIndex;
    memcpy(&initialCast, c, sizeof initialCast);
    
    fclose(f);
    
    // The file must end with a semi-colon preceding no other lexical 
    // item.
    return state.search != SEARCH_PREPARE;
}

typedef struct {
    char left, right;
} sCol;

static sCol collides(unsigned int x, unsigned int y, unsigned width, 
    sLevel const *l);
#define collides(x, y, w, l) collides((unsigned short)(x), \
    (unsigned short)(y), (unsigned short)(w), (l))

static void spawnActor(sCast *c, sActor a);
static void killActor(sCast *c, unsigned int id);

#define ABS(X) ~~((X)>0 ? (X) : -(X))
#define POS_TO_TILE_INDEX(X, Y, H) ~~((H)*((X)/TILE_PELS) + (Y)/TILE_PELS)
#define ACTOR_GRAVITY 1
#define ACTOR_MAX_SPEED_Y ~~(TILE_PELS-1)
#define ANIM_GAIT_TRANSITION_PERIOD 8U


#define PLAYER_WALK_COEF(VEL) ~~(1*(VEL)/ 2)
#define PLAYER_FRICTION 26
#define PLAYER_JUMP_HOLD_FRAMES 18
#define PLAYER_SLIDE_HOLD_FRAMES 32
#define PLAYER_JUMP_VEL 6
#define PLAYER_RESPAWN_FRAMES 120U
#define ANIM_DASH_SLIDE_TRANSITION 2U
#define ANIM_SLIDE_GETUP_TRANSITION 3U
#define ANIM_HURT_TRANSITION 8U
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

#define HUNTER_AIM_PERIOD 40U
#define HUNTER_RECOIL_PERIOD 10U
#define HUNTER_DAMAGE 1U
#define HUNTER_KNOCKBACK 200
#define ANIM_SHOOT_FLASH_TRANSITION 1U
enum {
    ANIM_HUNTER_WALK_NEUTRAL,
    ANIM_HUNTER_WALK_LEFT,
    ANIM_HUNTER_WALK_RIGHT,
    ANIM_HUNTER_RUN_NEUTRAL,
    ANIM_HUNTER_RUN_LEFT,
    ANIM_HUNTER_RUN_RIGHT,
    ANIM_HUNTER_DASH,
    ANIM_HUNTER_SLIDE,
    ANIM_HUNTER_GETUP,
    ANIM_HUNTER_HURT,
    ANIM_HUNTER_DEAD,
    ANIM_HUNTER_JUMP,
    ANIM_HUNTER_CROUCH,
    ANIM_HUNTER_AIM,
    ANIM_HUNTER_SHOOT_STANDING,
    ANIM_HUNTER_SHOOT_CROUCHING
};

#define NINGEN_SPAWN_PERIOD 50U
#define NINGEN_HURT_PERIOD 70U
#define NINGEN_SPAWN_AMOUNT 80U
#define NINGEN_DAMAGE 2

int updateNpc(sScene *s, sActor *a) {
    sMold const mold = s->md.data[a->moldId];
    sLevel const *level = &s->level;
    struct {
        sCol floor, step;
    } hitting;
    signed char const velX = (signed char)(a->vel.subX>>8);
    char facingRight = a->frame >= 0;
    unsigned char absFrame = (unsigned char) 
        (facingRight ? a->frame : ~a->frame);
    
    // Prevent the vertical displacement of the actor to cause 
    // arithmetic underflow when falling.
    if (a->vel.y < 0 && -a->vel.y > a->pos.y) {
        killActor(&s->cast, (unsigned int)(a - &s->cast.actorData.actor[0]));
        return 1;
        
    }
    
    a->pos.x = (unsigned short)(a->pos.x + velX);
    a->pos.y = (unsigned short)(a->pos.y + a->vel.y);
    hitting.floor = collides(a->pos.x, a->pos.y-1, mold.w, level);
    if (hitting.floor.left||hitting.floor.right) {
        a->pos.y = (unsigned short) (((a->pos.y+TILE_PELS-1)/TILE_PELS)
            * TILE_PELS);
        a->vel.y = 0;
        
    } else {
        if (a->vel.y > -ACTOR_MAX_SPEED_Y) {
            a->vel.y = (signed char) (a->vel.y - ACTOR_GRAVITY);
        
        } else {
            a->vel.y = -ACTOR_MAX_SPEED_Y;
            
        }
        
    }
    
    hitting.step = collides(a->pos.x, a->pos.y+TILE_PELS-1, mold.w, level);
    
    switch (a->moldId) {
        case MOLDID_HUNTER: {
            sMold const playerMold = s->md.data
                [s->cast.actorData.player.moldId];
            sActor const pa = s->cast.actorData.player;
            unsigned char seePlayer;
            
            if (absFrame != ANIM_HUNTER_AIM 
                    && absFrame != ANIM_HUNTER_SHOOT_STANDING
                    && absFrame != ANIM_HUNTER_DEAD) {
                if (facingRight) {
                    if (hitting.step.right) {
                        facingRight = 0;
                        
                    } else if (a->vel.subX < mold.maxSpeed << 8) {
                        a->vel.subX = (signed short) 
                            (a->vel.subX + mold.subAccel);
                        
                    }
                    
                } else {
                    if (hitting.step.left) {
                        facingRight = 1;
                        
                    } else if (a->vel.subX > -mold.maxSpeed << 8) {
                        a->vel.subX = (signed short) 
                            (a->vel.subX - mold.subAccel);
                        
                    }
                    
                }
                
            } else {
                if (a->vel.subX>>8 == 0) {
                    a->vel.subX = 0;
                    
                } else {
                    if (facingRight) {
                        a->vel.subX = (signed short) 
                            (a->vel.subX - PLAYER_FRICTION);
                        
                    } else {
                        a->vel.subX = (signed short) 
                            (a->vel.subX + PLAYER_FRICTION);
                        
                    }
                    
                }
                
            }
            
            if (a->health <= 0) {
                absFrame = ANIM_HUNTER_DEAD;
                
            } else if (a->pos.y >= pa.pos.y 
                    &&a->pos.y < pa.pos.y+playerMold.h
                    && ((facingRight && pa.pos.x+playerMold.w  
                    >= a->pos.x+mold.w/2U)
                    || (!facingRight && pa.pos.x < a->pos.x+mold.w/2U))
                    && pa.health > 0) {
                unsigned const int step = facingRight ? TILE_PELS :
                    (unsigned int) -TILE_PELS;
                unsigned int x, y, start, boundary;
                
                start = a->pos.x + mold.w/2U;
                y = a->pos.y + mold.h/2U;
                seePlayer = 1;
                
                // XXX: Add case where the boundary goes beyond the 
                // level.
                if (pa.pos.x >= VIEWPORT_WIDTH/2U) {
                    boundary = start + (facingRight ? VIEWPORT_WIDTH/2U
                        : (unsigned int) -(VIEWPORT_WIDTH/2U));
                    
                } else {
                    boundary = facingRight ? VIEWPORT_WIDTH/2U : 0;
                    
                }
                
                for (x = start; 
                        facingRight ? x<boundary : x>boundary; 
                        x += step) {
                    TILE t;
                    
                    if (facingRight) {
                        if (x > pa.pos.x) {
                            break;
                            
                        }
                        
                    } else {
                        if (x < pa.pos.x) {
                            break;
                            
                        }
                        
                    }
                    
                    t = level->data[POS_TO_TILE_INDEX(x, y, level->h)];
                    
                    // The actor is looking at solid tiles first. In this 
                    // case, the actor can skip the attacking logic.
                    if (t%SOLID_TILE_PERIOD == 0) {
                        seePlayer = 0;
                        break;
                        
                    }
                }
                
            } else {
                seePlayer = 0;
                
            }
            
            // Logic that makes decisions using the enemy's 
            // animation frame can change said animation frame.
            if (seePlayer) {
                if (absFrame!=ANIM_HUNTER_AIM) {
                    absFrame = ANIM_HUNTER_AIM;
                    a->timer = 1U;
                    
                }
                
            }
            
            switch (absFrame) {
                case ANIM_HUNTER_WALK_NEUTRAL: {
                    if (a->timer % ANIM_GAIT_TRANSITION_PERIOD == 0) {
                        if (a->timer % (3U*ANIM_GAIT_TRANSITION_PERIOD) == 0) {
                            absFrame = ANIM_HUNTER_WALK_LEFT;
                            
                        } else {
                            absFrame = ANIM_HUNTER_WALK_RIGHT;
                            
                        }
                        
                    }
                    ++a->timer;
                    break;
                    
                }
                
                case ANIM_HUNTER_WALK_RIGHT:
                case ANIM_HUNTER_WALK_LEFT: {
                    if (a->timer % ANIM_GAIT_TRANSITION_PERIOD == 0) {
                        absFrame = ANIM_HUNTER_WALK_NEUTRAL;
                        
                    }
                    ++a->timer;
                    break;
                    
                }
                
                case ANIM_HUNTER_SHOOT_STANDING:
                case ANIM_HUNTER_AIM: {
                    if (((a->timer == HUNTER_AIM_PERIOD) && pa.health > 0)
                            || (pa.pos.x > a->pos.x
                            && pa.pos.x < a->pos.x+mold.w)
                            || (pa.pos.x+playerMold.w > a->pos.x
                            && pa.pos.x+playerMold.w < a->pos.x+mold.w)) {
                        
                        // Only damage the player if the player is 
                        // within sight.
                        if (seePlayer) {
                            sActor *p = &s->cast.actorData.player;
                            p->health = (signed char)
                                (s->cast.actorData.player.health 
                                - (signed char) HUNTER_DAMAGE);
                            if (pa.pos.x > a->pos.x) {
                                p->frame = ANIM_PLAYER_HURT;
                                p->vel.subX = (signed short)
                                    (p->vel.subX + HUNTER_KNOCKBACK);
                                
                            } else {
                                p->frame = ~ANIM_PLAYER_HURT;
                                p->vel.subX = (signed short)
                                    (p->vel.subX - HUNTER_KNOCKBACK);
                                
                            }
                            
                            // Do not stack the vertical component of 
                            // the knockback.
                            p->vel.y = +6;
                            
                            if (a->timer >= HUNTER_AIM_PERIOD) {
                                absFrame = ANIM_HUNTER_SHOOT_STANDING;
                            
                            }
                                
                            
                        }
                        
                    } else if (a->timer > HUNTER_AIM_PERIOD) {
                        absFrame = ANIM_HUNTER_AIM;
                        if (a->timer > HUNTER_AIM_PERIOD
                                + ANIM_SHOOT_FLASH_TRANSITION
                                + HUNTER_RECOIL_PERIOD) {
                            a->timer = 0;
                            absFrame = ANIM_HUNTER_WALK_NEUTRAL;
                            
                        }
                        
                    }
                    
                    ++a->timer;
                    break;
                    
                }
                
                case ANIM_HUNTER_DEAD:
                default:
            }
            
            break;
            
        }
        
        case MOLDID_NINGEN: {
            if (a->timer % NINGEN_SPAWN_PERIOD == 0) {
                unsigned int iterations, i;
                unsigned int const step = mold.w/NINGEN_SPAWN_AMOUNT,
                    maxActors = (unsigned int) 
                    ARRAY_ELEMENTS(s->cast.actorData.actor);
                
                if (s->cast.actors == maxActors) {
                    unsigned int kills;
                    for (i = 0, kills = 0; 
                            i < maxActors && kills < NINGEN_SPAWN_AMOUNT;
                            ++i) {
                        if (s->cast.actorData.actor[i].moldId 
                                == MOLDID_HUNTER) {
                            killActor(&s->cast, i);
                            ++kills;
                            
                        }
                    }
                    iterations = kills;
                    
                } else if (s->cast.actors+NINGEN_SPAWN_AMOUNT > maxActors) {
                    iterations = (unsigned int) (maxActors - s->cast.actors);
                    
                    
                } else {
                    iterations = NINGEN_SPAWN_AMOUNT;
                    
                }
                    
                for (i = 0; i < iterations; ++i) {
                    sActor npc;
                    npc.pos.x = (unsigned short) (a->pos.x + i*step);
                    npc.pos.y = (unsigned short) a->pos.y;
                    npc.moldId = MOLDID_HUNTER;
                    npc.frame = i%2 == 0 ?  ANIM_HUNTER_WALK_NEUTRAL
                        : ~ANIM_HUNTER_WALK_NEUTRAL;
                    npc.health = +1;
                    npc.timer = 0;
                    npc.vel.subX = 0;
                    npc.vel.y = 0;
                    
                    spawnActor(&s->cast, npc);
                }
                
            }
            
            ++a->timer;
            break;
            
        }
        
        case MOLDID_PLAYER:
        default:
    }
    
    // Apply horizontal collision checks after the enemy updates in 
    // function of their behaviour.
    if (hitting.step.left) {
        a->pos.x = (unsigned short)(((a->pos.x + TILE_PELS-1) / TILE_PELS)
            * TILE_PELS);
        
        a->vel.subX = 0;
        
    } else if (hitting.step.right) {
        a->pos.x = (unsigned short)(((a->pos.x + mold.w-1) / TILE_PELS)
            * TILE_PELS - mold.w-1);
        a->vel.subX = 0;
        
    } else {
        if (a->vel.subX > mold.maxSpeed << 8) {
            a->vel.subX = (signed short) (mold.maxSpeed << 8);
            
        } else if (a->vel.subX < -mold.maxSpeed << 8) {
            a->vel.subX = (signed short) (-mold.maxSpeed << 8);
            
        }
    
    }
    
    if (facingRight) {
        a->frame = (signed char) absFrame;
        
    } else {
        a->frame = (signed char) ~absFrame;
        
    }
    
    return 0;
}

sActor updatePlayer(sContext *c) {
    sActor p = c->scene.cast.actorData.player;
    sMold const mold = c->scene.md.data[p.moldId];
    sLevel const level = c->scene.level;
    sInput input;
    sCoord const prev = p.pos;
    signed short maxSpeed;
    struct {
        sCol floor, step, bottom, ceil;
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
        c->scene.cast = initialCast;
        p = initialCast.actorData.player;
        
    }
    
    // This function deduces the player's orientation from its `frame` 
    // attribute. The function must perform manipulations on a copy of 
    // this value.
    if (p.frame < 0) {
        absFrame = (unsigned char) ~p.frame;
        
    } else {
        absFrame = (unsigned char) p.frame;
    
    }
    
    // Other actors pass messages to the player by changing the 
    // player's animation frame. It is the player's responsibility 
    // to process the change of their animation frame.
    memset(&input, 0x00, sizeof input);
    if (p.health > 0) {
        switch (absFrame) {
            case ANIM_PLAYER_HURT: {
                
                // Prevent the player from jumping while taking damage.
                input.jump.holdDur = PLAYER_JUMP_HOLD_FRAMES;
                
                // Make knockback take full effect.
                input.run.holdDur = 1;
                
                if (p.timer++ < ANIM_HURT_TRANSITION) {
                    p.timer = 0;
                    
            default:
                    input.up = c->input.up;
                    input.slide = c->input.slide;
                    input.left = c->input.left;
                    input.right = c->input.right;
                
            case ANIM_PLAYER_CROUCH:
                    input.jump = c->input.jump;
                    input.run = c->input.run;
                    input.down = c->input.down;
                
                }
                break;
                
            }
        }
        
    } else {
        if (absFrame == ANIM_PLAYER_DEAD) {
            if (p.timer++ == PLAYER_RESPAWN_FRAMES) {
                c->scene.cast = initialCast;
                p = initialCast.actorData.player;
                
            }
            
            // End prematurely because there is no logic to process 
            // when dead.
            return p;
            
        } else if (absFrame == ANIM_PLAYER_WALK_NEUTRAL) {
            absFrame = ANIM_PLAYER_DEAD;
            p.timer = 0;
            
        }
        // Otherwise, pretend as if the player did not input 
        // anything.
        
    }
    
    if (input.run.holdDur) {
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
    // they are jumping in place.
    hitting.floor = collides(p.pos.x, p.pos.y-1, mold.w, &level);
    hitting.step = collides(p.pos.x, p.pos.y+TILE_PELS-1, mold.w, &level);
    if (((hitting.floor.left||hitting.floor.right)&&p.vel.y==0)
            || (p.vel.y <= -PLAYER_JUMP_VEL
            &&( ((hitting.floor.left&&!hitting.step.left)
            ||(hitting.floor.right&&!hitting.step.right)) ))) {
        if (input.jump.holdDur == 1) {
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
            if (input.slide.holdDur > 0 
                    && input.slide.holdDur < PLAYER_SLIDE_HOLD_FRAMES) {
                signed short const slidingSpeed = (signed short)
                    (mold.maxSpeed << 8);
                
                if (input.left.holdDur && (p.vel.subX == -slidingSpeed
                        || input.slide.holdDur == 1)) {
                    p.vel.subX = (signed short) -slidingSpeed;
                    maxSpeed =  slidingSpeed;
                    
                } else if (input.right.holdDur 
                        && (p.vel.subX == slidingSpeed
                        || input.slide.holdDur == 1)) {
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
                    
                } else if (((p.vel.subX > 0&&input.left.holdDur)
                        ||(p.vel.subX < 0&&input.right.holdDur))
                        && input.run.holdDur) {
                    absFrame = ANIM_PLAYER_DASH;
                    
                } else if (p.timer%changePeriod == 0) {
                    struct {
                        unsigned char left, right, neutral;
                    } frame;
                    
                    if (input.run.holdDur) {
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
            if (absFrame != ANIM_PLAYER_DEAD) {
                absFrame = ANIM_PLAYER_WALK_NEUTRAL;
                p.timer = 0;
                
            }
            
        }
        
        
    } else {
        hitting.ceil = collides(p.pos.x, p.pos.y+mold.h-1, mold.w, &level);
        
        if (hitting.ceil.left || hitting.ceil.right
                || (p.pos.y + mold.h-1) > TILE_PELS*level.h) {
            p.pos.y = (unsigned short)((p.pos.y/TILE_PELS)*TILE_PELS
                - (mold.h-TILE_PELS));
            p.vel.y = 0;
            
        } else {
            if (p.vel.y == +PLAYER_JUMP_VEL 
                    && input.jump.holdDur > 0
                    && input.jump.holdDur < PLAYER_JUMP_HOLD_FRAMES) {
                p.vel.y = PLAYER_JUMP_VEL;
                
            } else {
                p.vel.y = (signed char)(p.vel.y-ACTOR_GRAVITY);
                if (p.vel.y < -ACTOR_MAX_SPEED_Y) {
                    p.vel.y = -ACTOR_MAX_SPEED_Y;
                    
                }
                
            }
            
        }
        
        if (absFrame != ANIM_PLAYER_HURT) {
            absFrame = ANIM_PLAYER_JUMP;
            p.timer = 0;
            
        }
        
    }
    
    if (input.down.holdDur) {
        // The crouch animation frame trumps over all other animation 
        // frames except when taking damage.
        absFrame = ANIM_PLAYER_CROUCH;
        
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
            p.pos.x = (unsigned short)(p.pos.x - (TILE_PELS-mold.w));
            
        }
        
    }
    
    // Change the sub-velocity if necessary to calculate the current 
    // horizontal velocity.
    if (input.left.holdDur) {
        p.vel.subX = (signed short)(p.vel.subX - mold.subAccel);
        
    }
    if (input.right.holdDur) {
        p.vel.subX = (signed short)(p.vel.subX + mold.subAccel);
        
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

#undef collides
static sCol collides(unsigned int x, unsigned int y, unsigned int width, 
        sLevel const *l) {
    sCol const col = {
        l->data[POS_TO_TILE_INDEX(x, y, l->h)] 
            % SOLID_TILE_PERIOD == 0,
        l->data[POS_TO_TILE_INDEX(x + width-1, y, l->h)]
            % SOLID_TILE_PERIOD == 0 || x+width-1 > TILE_PELS*l->w
    };
    
    return col;
}

static void spawnActor(sCast *c, sActor a) {
    c->actorData.actor[c->actors++] = a;
    return;
}

static void killActor(sCast *c, unsigned int id) {
    c->actorData.actor[id] = c->actorData.actor[c->actors - 1];
    c->actorData.actor[--c->actors].moldId = MOLD_NULL;
    return;
}