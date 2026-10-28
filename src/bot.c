/*
 * Autopilot / level verifier.
 *
 * The bot is a best-first search over "jump here" decisions.  A plain run
 * (without jumping) tells us where the cube dies; every ground frame before
 * that point becomes a candidate jump.  Candidates are ranked by how far the
 * resulting run gets, and the search recurses into the most promising ones.
 * The resulting list of jump positions drives the demo mode (F1 / --autoplay)
 * and proves in CI that the level is beatable.
 */
#include "gd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PLAN      192
#define SNAP_STEP     3.0f          /* one search branch every 3 px on ground  */
#define MAX_SNAPS     8192
#define MAX_CANDIDATES 12           /* how many branches we recurse into       */
#define MIN_GAIN      12.0f         /* a jump must get us this much further    */
#define MAX_STEPS     12000000L

static float g_plan[MAX_PLAN];
static int   g_plan_len;
static long  g_budget;

typedef struct {
    float x, y, vy, rot;
} Snapshot;

typedef struct {
    float x;
    float reach;
    int   snap;
} Candidate;

typedef struct {
    float best;
    bool  finished;
} RunResult;

/* Runs the level from `start`, pressing jump only on the very first frame if
 * `jump_first` is set, and records every grounded frame into `snaps`. */
static RunResult run(const Level *l, Player start, bool jump_first,
                     Snapshot *snaps, int *snap_count, int snap_cap)
{
    RunResult res = { start.x, false };
    Player p = start;
    bool first = true;

    while (g_budget > 0) {
        g_budget--;
        bool hold = first && jump_first;
        first = false;

        bool ok = player_step(l, &p, hold, PHYS_DT);
        if (p.x > res.best) {
            res.best = p.x;
        }
        if (p.finished) {
            res.finished = true;
            break;
        }
        if (!ok) {
            break;
        }
        if (p.on_ground && snaps && *snap_count < snap_cap) {
            if (*snap_count == 0 ||
                p.x - snaps[*snap_count - 1].x >= SNAP_STEP) {
                Snapshot *s = &snaps[(*snap_count)++];
                s->x = p.x;
                s->y = p.y;
                s->vy = p.vy;
                s->rot = p.rot;
            }
        }
    }
    return res;
}

static int cmp_candidate(const void *a, const void *b)
{
    const Candidate *ca = (const Candidate *)a;
    const Candidate *cb = (const Candidate *)b;
    if (ca->reach < cb->reach) return 1;
    if (ca->reach > cb->reach) return -1;
    return 0;
}

static Player player_from(const Snapshot *s)
{
    Player p;
    memset(&p, 0, sizeof(p));
    p.x = s->x;
    p.y = s->y;
    p.vy = s->vy;
    p.rot = s->rot;
    p.on_ground = true;
    p.alive = true;
    return p;
}

/* Executes one jump from a grounded state and returns the state right after
 * it - this is what the search recurses into. */
static Player do_jump(const Level *l, Player from)
{
    from.alive = true;
    player_step(l, &from, true, PHYS_DT);
    return from;
}

static bool solve_rec(const Level *l, Player start, int depth, int max_jumps)
{
    Snapshot *snaps = (Snapshot *)malloc(sizeof(Snapshot) * MAX_SNAPS);
    Candidate *cands = (Candidate *)malloc(sizeof(Candidate) * MAX_SNAPS);
    if (!snaps || !cands) {
        free(snaps);
        free(cands);
        return false;
    }

    int snap_count = 0;
    RunResult plain = run(l, start, false, snaps, &snap_count, MAX_SNAPS);
    if (plain.finished) {
        g_plan_len = depth;
        free(snaps);
        free(cands);
        return true;
    }
    if (depth >= max_jumps || g_budget <= 0 || snap_count == 0) {
        free(snaps);
        free(cands);
        return false;
    }

    int n = 0;
    for (int i = snap_count - 1; i >= 0; i--) {
        Player from = player_from(&snaps[i]);
        RunResult probe = run(l, from, true, NULL, NULL, 0);
        if (probe.finished) {
            g_plan[depth] = snaps[i].x;
            g_plan_len = depth + 1;
            free(snaps);
            free(cands);
            return true;
        }
        if (probe.best > plain.best + MIN_GAIN && n < MAX_SNAPS) {
            cands[n].x = snaps[i].x;
            cands[n].reach = probe.best;
            cands[n].snap = i;
            n++;
        }
        if (g_budget <= 0) {
            break;
        }
    }

    qsort(cands, (size_t)n, sizeof(Candidate), cmp_candidate);

    if (getenv("GD_BOT_DEBUG")) {
        fprintf(stderr, "depth %d start x=%.1f plain death x=%.1f cands=%d:",
                depth, start.x, plain.best, n);
        for (int i = 0; i < n && i < 6; i++) {
            fprintf(stderr, " [%.0f->%.0f]", cands[i].x, cands[i].reach);
        }
        fprintf(stderr, "\n");
    }

    int tried = 0;
    float tried_x[MAX_CANDIDATES];
    for (int i = 0; i < n && tried < MAX_CANDIDATES; i++) {
        /* snapshots are dense, so skip jumps that start almost at the same
         * place as one we already tried */
        bool dup = false;
        for (int j = 0; j < tried; j++) {
            if (SDL_fabsf(cands[i].x - tried_x[j]) < 2.0f) {
                dup = true;
                break;
            }
        }
        if (dup) {
            continue;
        }

        Player from = do_jump(l, player_from(&snaps[cands[i].snap]));
        g_plan[depth] = cands[i].x;
        tried_x[tried++] = cands[i].x;
        if (!from.alive) {
            continue;
        }
        if (solve_rec(l, from, depth + 1, max_jumps)) {
            free(snaps);
            free(cands);
            return true;
        }
        if (g_budget <= 0) {
            break;
        }
    }

    free(snaps);
    free(cands);
    return false;
}

bool level_solve(const Level *l, float *jump_x, int max_jumps, int *jump_count,
                 long *nodes, float *reach)
{
    Player start;
    player_reset(&start, l);

    g_plan_len = 0;
    g_budget = MAX_STEPS;

    bool ok = solve_rec(l, start, 0, SDL_min(max_jumps, MAX_PLAN));

    if (jump_count) {
        *jump_count = ok ? g_plan_len : 0;
    }
    if (nodes) {
        *nodes = MAX_STEPS - g_budget;
    }
    if (reach) {
        long saved = g_budget;
        g_budget = 4000000L;
        RunResult r = run(l, start, false, NULL, NULL, 0);
        *reach = r.finished ? l->finish_x : r.best;
        g_budget = saved;
    }
    if (ok && jump_x) {
        memcpy(jump_x, g_plan, sizeof(float) * (size_t)g_plan_len);
    }
    return ok;
}
