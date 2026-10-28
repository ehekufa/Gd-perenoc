/*
 * Point of No Return - a Geometry Dash style game written in C with SDL3.
 *
 * Copyright (c) 2026 ehekufa. Licensed under the MIT license (see LICENSE).
 */
#ifndef GD_H
#define GD_H

#include <stdbool.h>
#include <SDL3/SDL.h>

#define GAME_TITLE   "Point of No Return"
#define LEVEL_NAME   "POINT OF NO RETURN"

/* Logical resolution, everything is authored for this and letterboxed. */
#define VIEW_W 1280
#define VIEW_H 720

/* One grid cell == one block. */
#define CELL 48.0f

/* Player hitbox is slightly smaller than the drawn cube (like GD does). */
#define PLAYER_HIT 42.0f
#define PLAYER_DRAW 48.0f

/* Physics (tuned so a full jump is ~2.7 blocks high, ~4.8 blocks long). */
#define RUN_SPEED      470.0f
#define GRAVITY       4700.0f
#define JUMP_VELOCITY (-1130.0f)
#define MAX_FALL_SPEED 2300.0f
#define SPIN_SPEED     700.0f
#define PHYS_DT        (1.0f / 240.0f)

/* Ground plane is y = 0, the world grows upwards (negative y). */
#define GROUND_Y 0.0f

typedef enum {
    OBJ_BLOCK,
    OBJ_SPIKE
} ObjKind;

typedef struct {
    ObjKind kind;
    float x, y;          /* top left corner of the cell rect          */
    float w, h;          /* full size of the object                   */
    float hx, hy, hw, hh;/* collision box                             */
} Obj;

typedef struct {
    Obj  *objs;
    int   count;
    int   cap;
    float finish_x;      /* x coordinate of the finish line           */
} Level;

typedef struct {
    float x, y;          /* top-left corner of the hitbox             */
    float vy;
    float rot;           /* degrees                                   */
    bool  on_ground;
    bool  alive;
    bool  finished;
    bool  was_airborne;  /* previous step state, used for dust FX     */
} Player;

#define MAX_PARTICLES 768

typedef struct {
    float x, y, vx, vy;
    float size;
    float life, max_life;
    float rot, spin;
    Uint8 r, g, b, a;
    bool  active;
} Particle;

typedef struct {
    SDL_Window      *window;
    SDL_Renderer    *renderer;
    SDL_Texture     *tex_blocks;
    SDL_Texture     *tex_cube;
    SDL_Texture     *tex_white;
    SDL_Surface     *headless_surface;

    Level            level;
    Player           player;
    Particle         particles[MAX_PARTICLES];
    int              particle_cursor;

    float            cam_x, cam_y;
    float            shake_x, shake_y;
    float            shake;
    float            flash;           /* red death flash               */
    float            ground_scroll;

    int              state;           /* GameState                     */
    float            state_timer;
    int              attempts;
    float            best_progress;   /* 0..1                          */
    float            name_timer;      /* level name fade out           */
    float            run_time;
    float            phys_acc;        /* time of the current attempt   */
    float            total_time;      /* wall clock of the session     */
    bool             paused;
    bool             hold;            /* jump button is held           */
    bool             jump_pressed;    /* edge, consumed by update      */
    int              deaths;
    int              touches_down;

    /* solution found by the verifier, used by the demo/autoplay mode */
    bool             autoplay;
    const float     *auto_jumps;
    int              auto_jump_count;
    int              auto_jump_index;

    /* audio */
    SDL_AudioStream    *audio_stream;
    SDL_AudioDeviceID   audio_device;
    bool                audio_ok;
} Game;

enum {
    STATE_PLAY,
    STATE_DYING,
    STATE_DEAD,
    STATE_WIN
};

/* ---------------------------------------------------------------- level */
void  level_build(Level *level);
void  level_free(Level *level);
float level_length_blocks(const Level *level);

/* -------------------------------------------------------------- physics */
void  player_reset(Player *p, const Level *level);
/* returns true while the player is alive and running */
bool  player_step(const Level *level, Player *p, bool hold, float dt);
float player_progress(const Level *level, const Player *p);

/* ---------------------------------------------------------------- game  */
bool  game_init(Game *g, bool headless, int w, int h);
void  game_shutdown(Game *g);
void  game_update(Game *g, bool hold, float dt);
void  game_respawn(Game *g);
void  game_render(Game *g);
void  game_screenshot(Game *g, const char *path);

/* ---------------------------------------------------------------- fx    */
void  particles_burst(Game *g, float x, float y, int count, float power,
                      Uint8 r, Uint8 g_, Uint8 b, bool round);
void  particles_update(Game *g, float dt);
void  particles_draw(Game *g);
void  particles_clear(Game *g);

/* -------------------------------------------------------------- render  */
void  render_world(Game *g);
void  render_ui(Game *g);

/* ---------------------------------------------------------------- font  */
void  font_draw(SDL_Renderer *ren, float x, float y, float scale,
                const char *text, SDL_Color color);
void  font_draw_shadow(SDL_Renderer *ren, float x, float y, float scale,
                       const char *text, SDL_Color color);
float font_width(float scale, const char *text);
float font_height(float scale);

/* --------------------------------------------------------------- audio  */
void  audio_init(Game *g);
void  audio_shutdown(Game *g);
void  audio_play(Game *g, int id);
void  audio_update(Game *g, float dt);
enum { SFX_JUMP, SFX_DEATH, SFX_WIN, SFX_START, SFX_BLOCK };

/* ------------------------------------------------------------- assets  */
SDL_Texture *texture_load(SDL_Renderer *ren, const char *relpath);

/* ----------------------------------------------------------------- bot  */
/* Searches for a valid sequence of jumps that completes the level.       */
bool  level_solve(const Level *level, float *jump_x, int max_jumps,
                  int *jump_count, long *nodes, float *reach);

/* ---------------------------------------------------------------- util  */
Uint32 rng_next(void);
float  rng_float(float a, float b);
void   draw_rect(SDL_Renderer *ren, float x, float y, float w, float h);
void   draw_rect_blend(SDL_Renderer *ren, float x, float y, float w, float h,
                       Uint8 r, Uint8 g_, Uint8 b, Uint8 a);
void   draw_gradient(SDL_Renderer *ren, float x, float y, float w, float h,
                     SDL_Color top, SDL_Color bottom);

#endif /* GD_H */
