/*
 * Game state, particles and the whole visual presentation.
 */
#include "gd.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define GROUND_SCREEN_Y 548.0f
#define CAM_LEAD        0.30f      /* player sits at 30% of the screen */

#define COL_ACCENT  ((SDL_Color){ 255, 214,  10, 255 })
#define COL_WHITE   ((SDL_Color){ 245, 246, 255, 255 })
#define COL_DIM     ((SDL_Color){ 150, 156, 200, 255 })

static float sx(const Game *g, float x) { return x - g->cam_x + g->shake_x; }
static float sy(const Game *g, float y) { return y + GROUND_SCREEN_Y + g->shake_y; }

/* ------------------------------------------------------------------ fx */

void particles_clear(Game *g)
{
    memset(g->particles, 0, sizeof(g->particles));
    g->particle_cursor = 0;
}

void particles_burst(Game *g, float x, float y, int count, float power,
                     Uint8 r, Uint8 g_, Uint8 b, bool round)
{
    for (int i = 0; i < count; i++) {
        Particle *p = &g->particles[g->particle_cursor];
        g->particle_cursor = (g->particle_cursor + 1) % MAX_PARTICLES;

        float ang = round ? rng_float(0.0f, 6.2831853f)
                          : rng_float(-3.14159f, 0.0f);
        float spd = rng_float(0.15f, 1.0f) * power;
        memset(p, 0, sizeof(*p));
        p->active = true;
        p->x = x + rng_float(-6.0f, 6.0f);
        p->y = y + rng_float(-6.0f, 6.0f);
        p->vx = cosf(ang) * spd;
        p->vy = sinf(ang) * spd;
        p->size = rng_float(3.0f, 9.0f);
        p->max_life = p->life = rng_float(0.35f, 0.85f);
        p->rot = rng_float(0.0f, 360.0f);
        p->spin = rng_float(-540.0f, 540.0f);
        p->r = r;
        p->g = g_;
        p->b = b;
        p->a = 255;
    }
}

void particles_update(Game *g, float dt)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (!p->active) {
            continue;
        }
        p->life -= dt;
        if (p->life <= 0.0f) {
            p->active = false;
            continue;
        }
        p->vy += 900.0f * dt;
        p->vx *= (1.0f - 1.6f * dt);
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->rot += p->spin * dt;
        p->a = (Uint8)(255.0f * SDL_max(0.0f, p->life / p->max_life));
    }
}

void particles_draw(Game *g)
{
    SDL_Renderer *ren = g->renderer;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (!p->active) {
            continue;
        }
        float size = p->size * (0.35f + 0.65f * (p->life / p->max_life));
        SDL_SetRenderDrawColor(ren, p->r, p->g, p->b, p->a);
        SDL_FRect r = { sx(g, p->x) - size * 0.5f, sy(g, p->y) - size * 0.5f,
                        size, size };
        SDL_RenderFillRect(ren, &r);
    }
}

/* --------------------------------------------------------------- assets */

static SDL_Texture *make_white_texture(SDL_Renderer *ren)
{
    SDL_Surface *s = SDL_CreateSurface(4, 4, SDL_PIXELFORMAT_RGBA32);
    if (!s) {
        return NULL;
    }
    SDL_FillSurfaceRect(s, NULL, SDL_MapRGBA(SDL_GetPixelFormatDetails(s->format),
                                             NULL, 255, 255, 255, 255));
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    SDL_DestroySurface(s);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

bool game_init(Game *g, bool headless, int w, int h,
               const char *level_path)
{
    memset(g, 0, sizeof(*g));
    g->best_progress = 0.0f;
    g->state = STATE_PLAY;
    g->level_path = (level_path && *level_path) ? level_path : DEFAULT_LEVEL;

    if (headless) {
        g->headless_surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_XRGB8888);
        if (!g->headless_surface) {
            return false;
        }
        g->renderer = SDL_CreateSoftwareRenderer(g->headless_surface);
    } else {
        g->window = SDL_CreateWindow(GAME_TITLE, w, h,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!g->window) {
            return false;
        }
        SDL_SetWindowMinimumSize(g->window, 640, 360);
        g->renderer = SDL_CreateRenderer(g->window, NULL);
    }
    if (!g->renderer) {
        return false;
    }
    SDL_SetRenderVSync(g->renderer, 1);
    SDL_SetRenderLogicalPresentation(g->renderer, VIEW_W, VIEW_H,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    g->tex_blocks = texture_load(g->renderer, "textures/blocks.png");
    g->tex_cube = texture_load(g->renderer, "textures/cube.png");
    /* The spike sprite is optional: drop any of these files into
     * assets/textures/ and it is used for the spikes.  Without one the spikes
     * are drawn procedurally. */
    static const char *spike_files[] = {
        "textures/Spike1.png",    /* Spike1.png,       the player's spike */
        "textures/Thorn1.png",    /* Thorn1.png, same sprite, older name  */
        "textures/spike.png",
        "textures/spike1.png",
        "textures/thorn.png",
        "textures/Spike.png",
        "textures/spikes.png",
    };
    for (size_t i = 0; i < SDL_arraysize(spike_files) && !g->tex_spike; i++) {
        g->tex_spike = texture_load_optional(g->renderer, spike_files[i]);
    }
    g->tex_white = make_white_texture(g->renderer);

    if (!level_load(&g->level, g->level_path)) {
        SDL_Log("Falling back to the built in level");
        level_build(&g->level);
    }
    player_reset(&g->player, &g->level);
    particles_clear(g);

    if (!headless) {
        audio_init(g);
    }

    return true;
}

/* The autopilot solution is only needed by the demo mode (F1 / --autoplay),
 * and it takes a moment to compute, so it is done on demand. */
void game_compute_autopilot(Game *g)
{
    static JumpPoint jumps[MAX_AUTO_JUMPS];
    int count = 0;

    if (g->auto_jumps) {
        return;
    }
    if (level_solve(&g->level, jumps, MAX_AUTO_JUMPS, &count, NULL, NULL)) {
        g->auto_jumps = jumps;
        g->auto_jump_count = count;
        SDL_Log("Autopilot computed %d jumps", count);
    } else {
        SDL_Log("WARNING: autopilot could not beat the level");
    }
}

void game_shutdown(Game *g)
{
    audio_shutdown(g);
    if (g->tex_blocks) SDL_DestroyTexture(g->tex_blocks);
    if (g->tex_cube) SDL_DestroyTexture(g->tex_cube);
    if (g->tex_spike) SDL_DestroyTexture(g->tex_spike);
    if (g->tex_white) SDL_DestroyTexture(g->tex_white);
    if (g->renderer) SDL_DestroyRenderer(g->renderer);
    if (g->window) SDL_DestroyWindow(g->window);
    if (g->headless_surface) SDL_DestroySurface(g->headless_surface);
    level_free(&g->level);
    memset(g, 0, sizeof(*g));
}

void game_respawn(Game *g)
{
    player_reset(&g->player, &g->level);
    g->phys_acc = 0.0f;
    particles_clear(g);
    g->state = STATE_PLAY;
    g->state_timer = 0.0f;
    g->auto_jump_index = 0;
    g->phys_step = 0;
    g->run_time = 0.0f;
    g->attempts++;
    g->flash = 0.0f;
    g->shake = 0.0f;
    audio_play(g, SFX_START);
}

/* --------------------------------------------------------------- update */

static void kill_player(Game *g)
{
    Player *p = &g->player;
    particles_burst(g, p->x + PLAYER_HIT * 0.5f, p->y + PLAYER_HIT * 0.5f,
                    46, 620.0f, 235, 240, 255, true);
    particles_burst(g, p->x + PLAYER_HIT * 0.5f, p->y + PLAYER_HIT * 0.5f,
                    26, 380.0f, 120, 200, 255, true);
    g->state = STATE_DYING;
    g->state_timer = 0.0f;
    g->flash = 1.0f;
    g->shake = 16.0f;
    g->deaths++;
    audio_play(g, SFX_DEATH);

    float prog = player_progress(&g->level, p);
    if (prog > g->best_progress) {
        g->best_progress = prog;
    }
}

void game_update(Game *g, bool hold, float dt)
{
    if (dt > 0.1f) {
        dt = 0.1f;
    }
    g->total_time += dt;
    g->name_timer += dt;
    g->flash = SDL_max(0.0f, g->flash - dt * 2.2f);
    g->shake = SDL_max(0.0f, g->shake - dt * 42.0f);

    if (g->state == STATE_DYING) {
        g->state_timer += dt;
        particles_update(g, dt);
        if (g->state_timer > 0.55f) {
            game_respawn(g);
        }
        return;
    }

    if (g->state == STATE_WIN) {
        g->state_timer += dt;
        particles_update(g, dt);
        if (g->state_timer > 0.35f && (rng_next() % 12) == 0) {
            particles_burst(g, g->player.x + rng_float(0.0f, VIEW_W * 0.6f),
                            g->player.y - rng_float(0.0f, 260.0f), 4, 260.0f,
                            (Uint8)(120 + rng_next() % 135),
                            (Uint8)(120 + rng_next() % 135), 255, true);
        }
        return;
    }

    if (g->state != STATE_PLAY || g->paused) {
        return;
    }

    g->run_time += dt;

    /* fixed timestep so the physics never depends on the frame rate */
    g->phys_acc += dt;
    int guard = 0;
    while (g->phys_acc >= PHYS_DT && guard++ < 64) {
        g->phys_acc -= PHYS_DT;

        /* autopilot: press jump on the exact physics step the solver used */
        bool step_hold = hold;
        if (g->autoplay) {
            step_hold = false;
            while (g->auto_jumps &&
                   g->auto_jump_index < g->auto_jump_count &&
                   g->auto_jumps[g->auto_jump_index].step < g->phys_step) {
                /* we somehow missed a decision, skip it instead of stalling */
                g->auto_jump_index++;
            }
            if (g->auto_jumps &&
                g->auto_jump_index < g->auto_jump_count &&
                g->auto_jumps[g->auto_jump_index].step == g->phys_step) {
                step_hold = true;
                g->auto_jump_index++;
            }
        }
        g->phys_step++;

        bool was_ground = g->player.on_ground;
        bool ok = player_step(&g->level, &g->player, step_hold, PHYS_DT);

        if (ok && !g->player.finished && was_ground && !g->player.on_ground) {
            audio_play(g, SFX_JUMP);
            particles_burst(g, g->player.x + PLAYER_HIT * 0.5f,
                            g->player.y + PLAYER_HIT, 7, 180.0f, 200, 230, 255, false);
        }
        if (ok && !was_ground && g->player.on_ground) {
            particles_burst(g, g->player.x + PLAYER_HIT * 0.5f,
                            g->player.y + PLAYER_HIT, 4, 120.0f, 180, 200, 240, false);
        }
        if (!ok) {
            kill_player(g);
            return;
        }
        if (g->player.finished) {
            g->state = STATE_WIN;
            g->state_timer = 0.0f;
            g->best_progress = 1.0f;
            audio_play(g, SFX_WIN);
            particles_burst(g, g->player.x + PLAYER_HIT * 0.5f,
                            g->player.y + PLAYER_HIT * 0.5f, 60, 700.0f,
                            255, 214, 10, true);
            return;
        }
    }

    /* camera */
    float target = g->player.x - VIEW_W * CAM_LEAD;
    if (target > g->cam_x) {
        g->cam_x += (target - g->cam_x) * SDL_min(1.0f, dt * 14.0f);
        if (target - g->cam_x < 1.0f) {
            g->cam_x = target;
        }
    }

    particles_update(g, dt);
    audio_update(g, dt);
}

/* ---------------------------------------------------------------- render */

static void draw_background(Game *g)
{
    SDL_Renderer *ren = g->renderer;

    SDL_Color top = { 26, 16, 48, 255 };
    SDL_Color bottom = { 8, 6, 20, 255 };
    draw_gradient(ren, 0, 0, VIEW_W, VIEW_H, top, bottom);

    /* glowing horizon, just above the ground line */
    SDL_Color skin_top = { 40, 22, 68, 0 };
    SDL_Color skin_mid = { 62, 34, 104, 150 };
    SDL_Color skin_bottom = { 20, 13, 40, 220 };
    draw_gradient(ren, 0, 400, VIEW_W, 88, skin_top, skin_mid);
    draw_gradient(ren, 0, 488, VIEW_W, 60, skin_mid, skin_bottom);

    /* parallax checker */
    float par = g->cam_x * 0.25f;
    float cell = 128.0f;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    for (int i = -1; i < VIEW_W / (int)cell + 2; i++) {
        for (int j = 0; j < 5; j++) {
            if ((i + j) & 1) {
                continue;
            }
            float x = fmodf((float)i * cell - par, cell * (VIEW_W / (int)cell + 2));
            if (x < -cell) continue;
            SDL_SetRenderDrawColor(ren, 120, 90, 200, 10);
            draw_rect(ren, x, 40.0f + j * cell * 0.9f, cell - 4, cell * 0.9f - 4);
        }
    }

    /* floating neon squares, deep parallax */
    for (int i = 0; i < 18; i++) {
        /* decorate: scattered, but deterministic */
        float hx = (float)((i * 977) % 1009) / 1009.0f;
        float hy = (float)((i * 613) % 577) / 577.0f;
        float hs = (float)((i * 431) % 233) / 233.0f;
        float speed = 0.06f + hy * 0.12f;
        float x = fmodf(hx * 2400.0f - g->cam_x * speed, 2400.0f);
        while (x < -100.0f) {
            x += 2400.0f;
        }
        float y = 50.0f + hy * 340.0f;
        float s = 12.0f + hs * 34.0f;
        Uint8 a = (Uint8)(16 + 26 * hs);
        SDL_SetRenderDrawColor(ren, 130, 110, 255, a);
        draw_rect(ren, x, y + sinf(g->total_time * (0.4f + hs)) * 10.0f, s, s);
    }
}

static void draw_ground(Game *g)
{
    SDL_Renderer *ren = g->renderer;

    SDL_Color gtop = { 22, 16, 40, 255 };
    SDL_Color gbot = { 6, 4, 14, 255 };
    draw_gradient(ren, 0, GROUND_SCREEN_Y, VIEW_W, VIEW_H - GROUND_SCREEN_Y,
                  gtop, gbot);

    /* the bright line that the cube runs on */
    SDL_SetRenderDrawColor(ren, 235, 240, 255, 255);
    draw_rect(ren, 0, GROUND_SCREEN_Y - 3.0f, VIEW_W, 3.0f);
    SDL_SetRenderDrawColor(ren, 120, 180, 255, 90);
    draw_rect(ren, 0, GROUND_SCREEN_Y - 12.0f, VIEW_W, 10.0f);

    /* scrolling grid */
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 90, 110, 200, 70);
    float off = fmodf(g->cam_x, CELL);
    for (float x = -off; x < VIEW_W; x += CELL) {
        SDL_FRect r = { x, GROUND_SCREEN_Y + 8.0f, 1.0f, VIEW_H - GROUND_SCREEN_Y };
        SDL_RenderFillRect(ren, &r);
    }
    for (float y = GROUND_SCREEN_Y + 32.0f; y < VIEW_H; y += 32.0f) {
        SDL_FRect r = { 0, y, VIEW_W, 1.0f };
        SDL_RenderFillRect(ren, &r);
    }
}

static void draw_block(Game *g, const Obj *o)
{
    SDL_Renderer *ren = g->renderer;
    float x = sx(g, o->x), y = sy(g, o->y);
    if (x > VIEW_W + 64 || x + o->w < -64) {
        return;
    }

    /* soft shadow */
    draw_rect_blend(ren, x + 5, y + 7, o->w, o->h, 0, 0, 0, 90);

    int cols = (int)(o->w / CELL + 0.5f);
    int rows = (int)(o->h / CELL + 0.5f);
    for (int cx = 0; cx < cols; cx++) {
        for (int cy = 0; cy < rows; cy++) {
            SDL_FRect dst = { x + cx * CELL, y + cy * CELL, CELL, CELL };
            if (g->tex_blocks) {
                SDL_RenderTexture(ren, g->tex_blocks, NULL, &dst);
            } else {
                SDL_SetRenderDrawColor(ren, 90, 90, 100, 255);
                SDL_RenderFillRect(ren, &dst);
            }
            /* deep blue tint so the plate matches the level's palette */
            draw_rect_blend(ren, dst.x, dst.y, dst.w, dst.h, 56, 74, 150, 120);
            /* dark tile grid, GD style */
            SDL_SetRenderDrawColor(ren, 24, 28, 56, 255);
            SDL_FRect cell_border = { dst.x + 0.5f, dst.y + 0.5f,
                                      dst.w - 1.0f, dst.h - 1.0f };
            SDL_RenderRect(ren, &cell_border);
            /* bevel: light from the top left, shadow at the bottom */
            draw_rect_blend(ren, dst.x + 2, dst.y + 2, dst.w - 4, 2, 210, 230, 255, 90);
            draw_rect_blend(ren, dst.x + dst.w - 4, dst.y + 2, 2, dst.h - 4, 0, 0, 0, 60);
            draw_rect_blend(ren, dst.x + 2, dst.y + dst.h - 4, dst.w - 4, 2, 0, 0, 0, 80);
        }
    }

    /* crisp outer outline */
    SDL_SetRenderDrawColor(ren, 14, 16, 36, 255);
    SDL_FRect border = { x - 1, y - 1, o->w + 2, o->h + 2 };
    SDL_RenderRect(ren, &border);
    SDL_SetRenderDrawColor(ren, 235, 242, 255, 255);
    draw_rect(ren, x, y, o->w, 3.0f);
    draw_rect(ren, x, y, 3.0f, o->h);
}

static void draw_spike(Game *g, const Obj *o)
{
    SDL_Renderer *ren = g->renderer;
    float x = sx(g, o->x), y = sy(g, o->y);
    if (x > VIEW_W + 64 || x + o->w < -64) {
        return;
    }
    float w = o->w, h = o->h;

    if (g->tex_spike) {
        /* the sprite is a cell sized triangle; keep a hair of padding so a
         * row of spikes still reads as separate triangles */
        SDL_FRect dst = { x + 1.0f, y, w - 2.0f, h };
        draw_rect_blend(ren, x + w * 0.15f, y + h * 0.55f, w * 0.7f, h * 0.45f,
                        0, 0, 0, 60);
        SDL_RenderTexture(ren, g->tex_spike, NULL, &dst);
        return;
    }

    SDL_Vertex v[3];
    int idx[3] = { 0, 1, 2 };

    v[0].position.x = x;         v[0].position.y = y + h;
    v[1].position.x = x + w;     v[1].position.y = y + h;
    v[2].position.x = x + w * 0.5f; v[2].position.y = y;

    v[0].color = (SDL_FColor){ 0.14f, 0.13f, 0.24f, 1.0f };
    v[1].color = (SDL_FColor){ 0.14f, 0.13f, 0.24f, 1.0f };
    v[2].color = (SDL_FColor){ 0.95f, 0.97f, 1.00f, 1.0f };
    for (int i = 0; i < 3; i++) {
        v[i].tex_coord.x = 0.0f;
        v[i].tex_coord.y = 0.0f;
    }
    SDL_RenderGeometry(ren, NULL, v, 3, idx, 3);

    /* outline */
    SDL_SetRenderDrawColor(ren, 235, 240, 255, 235);
    SDL_RenderLine(ren, x + 1, y + h, x + w * 0.5f, y);
    SDL_RenderLine(ren, x + w * 0.5f, y, x + w - 1, y + h);
    SDL_SetRenderDrawColor(ren, 60, 70, 110, 255);
    SDL_RenderLine(ren, x + 1, y + h, x + w - 1, y + h);
}

static void draw_finish(Game *g)
{
    SDL_Renderer *ren = g->renderer;
    float x = sx(g, g->level.finish_x);
    if (x > VIEW_W + 120 || x < -120) {
        return;
    }
    float top = sy(g, -CELL * 4.0f);
    float h = GROUND_SCREEN_Y - top;

    for (int i = 0; i < 10; i++) {
        float t = (float)i / 10.0f;
        Uint8 a = (Uint8)(70 + 60 * sinf(g->total_time * 4.0f + t * 6.0f));
        draw_rect_blend(ren, x - 18.0f - t * 22.0f, top, 22.0f, h,
                        255, 214, 10, (Uint8)(a / 4));
    }
    SDL_SetRenderDrawColor(ren, 255, 214, 10, 255);
    draw_rect(ren, x - 4.0f, top, 8.0f, h);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    draw_rect(ren, x - 1.0f, top, 2.0f, h);

    font_draw_shadow(ren, x - font_width(2.0f, "END") * 0.5f, top - 34.0f, 2.0f,
                     "END", COL_ACCENT);
}

static void draw_player(Game *g)
{
    SDL_Renderer *ren = g->renderer;
    Player *p = &g->player;
    float cx = sx(g, p->x) + PLAYER_HIT * 0.5f;
    float cy = sy(g, p->y) + PLAYER_HIT * 0.5f;

    draw_rect_blend(ren, cx - 30.0f, cy - 30.0f, 60.0f, 60.0f, 120, 200, 255,
                    (Uint8)(26 + 16 * sinf(g->total_time * 6.0f)));

    SDL_FRect dst = { cx - PLAYER_DRAW * 0.5f, cy - PLAYER_DRAW * 0.5f,
                      PLAYER_DRAW, PLAYER_DRAW };

    /* a coloured body behind the texture so the cube always reads well */
    SDL_Color body_top = { 165, 92, 255, 255 };
    SDL_Color body_bottom = { 60, 130, 255, 255 };
    draw_rect_blend(ren, dst.x, dst.y, dst.w, dst.h, body_top.r, body_top.g,
                    body_top.b, 255);
    draw_rect_blend(ren, dst.x, dst.y + dst.h * 0.55f, dst.w, dst.h * 0.45f,
                    body_bottom.r, body_bottom.g, body_bottom.b, 255);

    if (g->tex_cube) {
        SDL_RenderTextureRotated(ren, g->tex_cube, NULL, &dst, p->rot, NULL,
                                 SDL_FLIP_NONE);
    }

    SDL_SetRenderDrawColor(ren, 250, 250, 255, 255);
    SDL_FRect border = { dst.x - 1, dst.y - 1, dst.w + 2, dst.h + 2 };
    SDL_RenderRect(ren, &border);
}

void render_world(Game *g)
{
    SDL_Renderer *ren = g->renderer;

    if (g->shake > 0.0f) {
        g->shake_x = rng_float(-g->shake, g->shake);
        g->shake_y = rng_float(-g->shake, g->shake);
    } else {
        g->shake_x = g->shake_y = 0.0f;
    }
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    draw_background(g);
    draw_ground(g);

    /* objects */
    for (int i = 0; i < g->level.count; i++) {
        const Obj *o = &g->level.objs[i];
        if (o->kind == OBJ_BLOCK) {
            draw_block(g, o);
        }
    }
    draw_finish(g);
    for (int i = 0; i < g->level.count; i++) {
        const Obj *o = &g->level.objs[i];
        if (o->kind == OBJ_SPIKE) {
            draw_spike(g, o);
        }
    }

    if (g->state != STATE_DYING) {
        draw_player(g);
    }
    particles_draw(g);

    if (g->flash > 0.0f) {
        draw_rect_blend(ren, 0, 0, VIEW_W, VIEW_H, 255, 60, 60,
                        (Uint8)(150.0f * g->flash));
    }
    /* vignette */
    SDL_Color clear = { 0, 0, 0, 0 };
    SDL_Color dark = { 0, 0, 0, 150 };
    draw_gradient(ren, 0, 0, VIEW_W, 110, dark, clear);
    draw_gradient(ren, 0, VIEW_H - 90.0f, VIEW_W, 90, clear, dark);
}

static void draw_progress_bar(Game *g)
{
    SDL_Renderer *ren = g->renderer;
    float w = 420.0f, h = 12.0f;
    float x = (VIEW_W - w) * 0.5f, y = 26.0f;

    draw_rect_blend(ren, x - 3, y - 3, w + 6, h + 6, 0, 0, 0, 140);
    draw_rect_blend(ren, x, y, w, h, 40, 44, 70, 220);

    float prog = (g->state == STATE_WIN) ? 1.0f
                                         : player_progress(&g->level, &g->player);
    draw_rect_blend(ren, x, y, w * prog, h, 120, 220, 255, 255);
    draw_rect_blend(ren, x, y, w * prog, h * 0.4f, 255, 255, 255, 120);

    if (g->best_progress > 0.001f) {
        float bx = x + w * g->best_progress;
        draw_rect_blend(ren, bx - 1.5f, y - 4.0f, 3.0f, h + 8.0f, 255, 214, 10, 255);
    }

    char buf[64];
    SDL_snprintf(buf, sizeof(buf), "%d%%", (int)(prog * 100.0f + 0.5f));
    font_draw_shadow(ren, x + w + 14.0f, y - 2.0f, 2.0f, buf, COL_WHITE);
}

void render_ui(Game *g)
{
    SDL_Renderer *ren = g->renderer;
    char buf[128];

    draw_progress_bar(g);

    /* level name, fades away after a few seconds */
    float fade = 1.0f;
    if (g->name_timer > 2.6f) {
        fade = SDL_max(0.0f, 1.0f - (g->name_timer - 2.6f) / 0.9f);
    }
    if (fade > 0.0f && g->state == STATE_PLAY) {
        SDL_Color c = { 245, 246, 255, (Uint8)(255 * fade) };
        font_draw_shadow(ren, (VIEW_W - font_width(3.0f, g->level.name)) * 0.5f,
                         96.0f, 3.0f, g->level.name, c);
        SDL_snprintf(buf, sizeof(buf), "LEVEL 1");
        SDL_Color c2 = { 150, 190, 255, (Uint8)(220 * fade) };
        font_draw_shadow(ren, (VIEW_W - font_width(1.6f, buf)) * 0.5f, 148.0f,
                         1.6f, buf, c2);
    }

    /* attempt counter */
    if (g->state == STATE_PLAY || g->state == STATE_DYING) {
        SDL_snprintf(buf, sizeof(buf), "ATTEMPT %d", g->attempts);
        SDL_Color c = { 200, 205, 240, 210 };
        font_draw_shadow(ren, (VIEW_W - font_width(2.0f, buf)) * 0.5f, 62.0f,
                         2.0f, buf, c);
    }

    /* best and hint */
    SDL_snprintf(buf, sizeof(buf), "BEST %d%%", (int)(g->best_progress * 100.0f + 0.5f));
    font_draw_shadow(ren, 26.0f, 26.0f, 1.8f, buf, COL_DIM);

    if (g->state == STATE_PLAY) {
        const char *hint = SDL_GetHint(SDL_HINT_TOUCH_MOUSE_EVENTS) ? "" : "";
        (void)hint;
        font_draw_shadow(ren, 26.0f, VIEW_H - 46.0f, 1.6f,
                         "SPACE OR TAP TO JUMP", COL_DIM);
    }

    if (g->paused && g->state == STATE_PLAY) {
        draw_rect_blend(ren, 0, 0, VIEW_W, VIEW_H, 0, 0, 0, 160);
        font_draw_shadow(ren, (VIEW_W - font_width(4.0f, "PAUSED")) * 0.5f,
                         VIEW_H * 0.42f, 4.0f, "PAUSED", COL_WHITE);
        const char *sub = "TAP TO RESUME";
        font_draw_shadow(ren, (VIEW_W - font_width(2.0f, sub)) * 0.5f,
                         VIEW_H * 0.52f, 2.0f, sub, COL_DIM);
    }

    if (g->state == STATE_WIN) {
        draw_rect_blend(ren, 0, 0, VIEW_W, VIEW_H, 6, 4, 20, 120);
        float t = g->state_timer;
        float pop = SDL_min(1.0f, t * 3.0f);
        float scale = 5.0f * (0.7f + 0.3f * pop);
        const char *t1 = "LEVEL COMPLETE";
        font_draw_shadow(ren, (VIEW_W - font_width(scale, t1)) * 0.5f,
                         VIEW_H * 0.34f, scale, t1, COL_ACCENT);

        SDL_snprintf(buf, sizeof(buf), "ATTEMPTS: %d", g->attempts);
        font_draw_shadow(ren, (VIEW_W - font_width(2.0f, buf)) * 0.5f,
                         VIEW_H * 0.50f, 2.0f, buf, COL_WHITE);

        const char *t3 = "TAP TO PLAY AGAIN";
        SDL_Color c = { 200, 210, 255, (Uint8)(160 + 95 * (0.5f + 0.5f * sinf(t * 5.0f))) };
        font_draw_shadow(ren, (VIEW_W - font_width(2.0f, t3)) * 0.5f,
                         VIEW_H * 0.62f, 2.0f, t3, c);
    }
}

void game_render(Game *g)
{
    render_world(g);
    render_ui(g);
    if (!g->headless_surface) {
        SDL_RenderPresent(g->renderer);
    }
}

void game_screenshot(Game *g, const char *path)
{
    game_render(g);
    SDL_Surface *shot = SDL_RenderReadPixels(g->renderer, NULL);
    if (!shot) {
        SDL_Log("screenshot failed: %s", SDL_GetError());
        return;
    }
    if (SDL_SaveBMP(shot, path)) {
        SDL_Log("saved %s", path);
    } else {
        SDL_Log("could not save %s: %s", path, SDL_GetError());
    }
    SDL_DestroySurface(shot);
}
