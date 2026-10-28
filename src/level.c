/*
 * Level 1: "Point of No Return".
 *
 * The level is authored on a grid: column 0 is the spawn, row 0 is the row of
 * blocks that rests directly on the ground.  One cell is CELL (48) pixels.
 */
#include "gd.h"
#include <stdlib.h>
#include <string.h>

static void level_reserve(Level *l, int extra)
{
    if (l->count + extra <= l->cap) {
        return;
    }
    int cap = l->cap ? l->cap * 2 : 128;
    while (cap < l->count + extra) {
        cap *= 2;
    }
    Obj *o = (Obj *)realloc(l->objs, (size_t)cap * sizeof(Obj));
    if (!o) {
        SDL_OutOfMemory();
        return;
    }
    l->objs = o;
    l->cap = cap;
}

/* A solid block occupying `w` cells horizontally and `h` cells vertically,
 * with its bottom row at `row`. */
static void L_block(Level *l, int col, int row, int w, int h)
{
    level_reserve(l, 1);
    Obj *o = &l->objs[l->count++];
    memset(o, 0, sizeof(*o));
    o->kind = OBJ_BLOCK;
    o->w = w * CELL;
    o->h = h * CELL;
    o->x = col * CELL;
    o->y = -(float)(row + h) * CELL;
    o->hx = o->x;
    o->hy = o->y;
    o->hw = o->w;
    o->hh = o->h;
}

/* A spike sitting in the cell (col,row), pointing up. */
static void L_spike(Level *l, int col, int row)
{
    level_reserve(l, 1);
    Obj *o = &l->objs[l->count++];
    memset(o, 0, sizeof(*o));
    o->kind = OBJ_SPIKE;
    o->w = CELL;
    o->h = CELL;
    o->x = col * CELL;
    o->y = -(float)(row + 1) * CELL;
    /* Spikes have a small hitbox in the middle of the cell, GD style. */
    o->hw = CELL * 0.34f;
    o->hh = CELL * 0.52f;
    o->hx = o->x + (CELL - o->hw) * 0.5f;
    o->hy = o->y + (CELL - o->hh);
}

static void L_spikes(Level *l, int col, int row, int n)
{
    for (int i = 0; i < n; i++) {
        L_spike(l, col + i, row);
    }
}

/* A row of spikes with a block stair behind it - the classic "jump up" bit. */
static void L_stair(Level *l, int col, int width, int height)
{
    L_block(l, col, 0, width, height);
}

void level_free(Level *l)
{
    free(l->objs);
    l->objs = NULL;
    l->count = l->cap = 0;
}

float level_length_blocks(const Level *l)
{
    return l->finish_x / CELL;
}

void level_build(Level *l)
{
    memset(l, 0, sizeof(*l));

    /* ------------------------------------------------------------------
     * Section 1 - "the point of no return" : a calm intro, single spikes.
     * ------------------------------------------------------------------ */
    L_spike(l, 13, 0);
    L_spike(l, 19, 0);
    L_spikes(l, 25, 0, 2);
    L_spike(l, 32, 0);

    /* ------------------------------------------------------------------
     * Section 2 - little platforms: hop up, run, hop down.
     * ------------------------------------------------------------------ */
    L_stair(l, 38, 3, 1);
    L_spike(l, 43, 0);
    L_stair(l, 46, 4, 2);
    L_spikes(l, 52, 0, 2);
    L_stair(l, 57, 3, 1);
    L_spike(l, 62, 0);

    /* ------------------------------------------------------------------
     * Section 3 - the staircase. Up, across, and a long drop.
     * ------------------------------------------------------------------ */
    L_stair(l, 68, 3, 1);
    L_stair(l, 71, 3, 2);
    L_stair(l, 74, 6, 3);
    L_spike(l, 73, 0);
    L_spike(l, 82, 0);
    L_spikes(l, 87, 0, 2);
    L_stair(l, 93, 3, 1);

    /* ------------------------------------------------------------------
     * Section 4 - the spike field, the hardest part of the level.
     * ------------------------------------------------------------------ */
    L_spikes(l, 99, 0, 2);
    L_spike(l, 105, 0);
    L_spikes(l, 110, 0, 3);
    L_spike(l, 117, 0);
    L_spikes(l, 122, 0, 2);
    L_stair(l, 128, 4, 2);

    /* ------------------------------------------------------------------
     * Section 5 - pillars: jump on the blocks, spikes in between.
     * ------------------------------------------------------------------ */
    L_spike(l, 135, 0);
    L_stair(l, 140, 2, 1);
    L_spikes(l, 144, 0, 2);
    L_stair(l, 150, 3, 2);
    L_spike(l, 156, 0);
    L_stair(l, 161, 3, 1);
    L_spikes(l, 166, 0, 2);

    /* ------------------------------------------------------------------
     * Section 6 - last sprint into the finish.
     * ------------------------------------------------------------------ */
    L_spike(l, 173, 0);
    L_stair(l, 178, 4, 1);
    L_spikes(l, 185, 0, 3);
    L_stair(l, 192, 6, 2);
    L_spike(l, 200, 0);
    L_spikes(l, 205, 0, 2);

    l->finish_x = 214.0f * CELL;
}
