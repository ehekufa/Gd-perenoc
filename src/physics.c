/*
 * Player physics: running, jumping, sweeping collisions and death checks.
 */
#include "gd.h"
#include <math.h>
#include <string.h>

static bool overlap(float ax, float ay, float aw, float ah,
                    float bx, float by, float bw, float bh)
{
    const float eps = 0.01f;
    return ax + aw > bx + eps && ax < bx + bw - eps &&
           ay + ah > by + eps && ay < by + bh - eps;
}

static bool touches_solid(const Level *l, const Player *p)
{
    for (int i = 0; i < l->count; i++) {
        const Obj *o = &l->objs[i];
        if (o->kind != OBJ_BLOCK) {
            continue;
        }
        if (o->x > p->x + PLAYER_HIT + CELL || o->x + o->w < p->x - CELL) {
            continue;
        }
        if (overlap(p->x, p->y, PLAYER_HIT, PLAYER_HIT, o->hx, o->hy, o->hw, o->hh)) {
            return true;
        }
    }
    return false;
}

static bool touches_hazard(const Level *l, const Player *p)
{
    for (int i = 0; i < l->count; i++) {
        const Obj *o = &l->objs[i];
        if (o->kind != OBJ_SPIKE) {
            continue;
        }
        if (o->x > p->x + PLAYER_HIT + CELL || o->x + o->w < p->x - CELL) {
            continue;
        }
        if (overlap(p->x, p->y, PLAYER_HIT, PLAYER_HIT, o->hx, o->hy, o->hw, o->hh)) {
            return true;
        }
    }
    return false;
}

void player_reset(Player *p, const Level *l)
{
    (void)l;
    memset(p, 0, sizeof(*p));
    p->x = 6.0f * CELL;
    p->y = GROUND_Y - PLAYER_HIT;
    p->vy = 0.0f;
    p->rot = 0.0f;
    p->on_ground = true;
    p->alive = true;
    p->finished = false;
}

float player_progress(const Level *l, const Player *p)
{
    float start = 6.0f * CELL;
    float t = (p->x - start) / (l->finish_x - start);
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    return t;
}

/* Move the player by one fixed physics step.  Returns false when the player
 * died (or finished, which is reported through p->finished). */
bool player_step(const Level *l, Player *p, bool hold, float dt)
{
    if (!p->alive) {
        return false;
    }

    p->was_airborne = !p->on_ground;

    if (p->on_ground) {
        /* Snap the cube to a right angle while it is rolling. */
        float target = roundf(p->rot / 90.0f) * 90.0f;
        p->rot += (target - p->rot) * SDL_min(1.0f, dt * 18.0f);
        if (fabsf(target - p->rot) < 0.5f) {
            p->rot = target;
        }
        if (hold) {
            p->vy = JUMP_VELOCITY;
            p->on_ground = false;
        }
    } else {
        p->rot += SPIN_SPEED * dt;
    }

    if (!p->on_ground) {
        p->vy += GRAVITY * dt;
        if (p->vy > MAX_FALL_SPEED) {
            p->vy = MAX_FALL_SPEED;
        }
    }

    /* ---- vertical sweep ---- */
    p->y += p->vy * dt;
    p->on_ground = false;

    if (p->vy >= 0.0f) {
        for (int i = 0; i < l->count; i++) {
            const Obj *o = &l->objs[i];
            if (o->kind != OBJ_BLOCK) {
                continue;
            }
            if (!overlap(p->x, p->y, PLAYER_HIT, PLAYER_HIT, o->hx, o->hy, o->hw, o->hh)) {
                continue;
            }
            /* Only land on the block when we come from above. */
            if (p->y + PLAYER_HIT - p->vy * dt <= o->y + 1.0f) {
                p->y = o->y - PLAYER_HIT;
                p->vy = 0.0f;
                p->on_ground = true;
            }
        }
    } else {
        for (int i = 0; i < l->count; i++) {
            const Obj *o = &l->objs[i];
            if (o->kind != OBJ_BLOCK) {
                continue;
            }
            if (overlap(p->x, p->y, PLAYER_HIT, PLAYER_HIT, o->hx, o->hy, o->hw, o->hh)) {
                p->y = o->y + o->h;
                p->vy = 0.0f;
            }
        }
    }

    /* the ground */
    if (p->y + PLAYER_HIT >= GROUND_Y) {
        p->y = GROUND_Y - PLAYER_HIT;
        p->vy = 0.0f;
        p->on_ground = true;
    }

    /* ---- horizontal sweep ---- */
    p->x += RUN_SPEED * dt;

    if (touches_solid(l, p)) {
        p->alive = false;
        return false;
    }
    if (touches_hazard(l, p)) {
        p->alive = false;
        return false;
    }
    if (p->x + PLAYER_HIT * 0.5f >= l->finish_x) {
        p->finished = true;
        p->alive = true;
        return true;
    }
    return true;
}
