/*
 * Small helpers: RNG, rectangles and gradients.
 */
#include "gd.h"

static Uint64 g_rng_state = 0x2545F4914F6CDD1DULL;

Uint32 rng_next(void)
{
    /* xorshift64* */
    g_rng_state ^= g_rng_state >> 12;
    g_rng_state ^= g_rng_state << 25;
    g_rng_state ^= g_rng_state >> 27;
    return (Uint32)((g_rng_state * 2685821657736338717ULL) >> 32);
}

float rng_float(float a, float b)
{
    float t = (float)(rng_next() & 0xFFFFFF) / (float)0xFFFFFF;
    return a + (b - a) * t;
}

void draw_rect(SDL_Renderer *ren, float x, float y, float w, float h)
{
    SDL_FRect r = { x, y, w, h };
    SDL_RenderFillRect(ren, &r);
}

void draw_rect_blend(SDL_Renderer *ren, float x, float y, float w, float h,
                     Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, r, g, b, a);
    draw_rect(ren, x, y, w, h);
}

void draw_gradient(SDL_Renderer *ren, float x, float y, float w, float h,
                   SDL_Color top, SDL_Color bottom)
{
    SDL_Vertex v[4];
    int idx[6] = { 0, 1, 2, 2, 3, 0 };

    for (int i = 0; i < 4; i++) {
        v[i].color.r = 1.0f;
        v[i].color.g = 1.0f;
        v[i].color.b = 1.0f;
        v[i].color.a = 1.0f;
        v[i].tex_coord.x = 0.0f;
        v[i].tex_coord.y = 0.0f;
    }
    v[0].position.x = x;         v[0].position.y = y;
    v[1].position.x = x + w;     v[1].position.y = y;
    v[2].position.x = x + w;     v[2].position.y = y + h;
    v[3].position.x = x;         v[3].position.y = y + h;

    SDL_Color c[2] = { top, bottom };
    for (int i = 0; i < 4; i++) {
        SDL_Color cc = (i < 2) ? c[0] : c[1];
        v[i].color.r = (float)cc.r / 255.0f;
        v[i].color.g = (float)cc.g / 255.0f;
        v[i].color.b = (float)cc.b / 255.0f;
        v[i].color.a = (float)cc.a / 255.0f;
    }
    SDL_RenderGeometry(ren, NULL, v, 4, idx, 6);
}
