/*
 * Autopilot / level verifier.
 *
 * The bot searches for a sequence of jumps that carries the cube to the end of
 * the level.  It is a beam search over "jump here" decisions:
 *
 *   1. from every state in the beam, run forward without jumping until the
 *      cube dies and remember all the moments it was touching the ground,
 *   2. jump at those moments, keep the jumps that get us the furthest,
 *   3. repeat with the best states as the new beam.
 *
 * The resulting list of jump positions drives the demo mode (F1 / --autoplay)
 * and, more importantly, proves in CI that the level is beatable.
 */
#include "gd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PLAN       96
#define SNAP_STEP      6.0f       /* one candidate every 6 px on the ground  */
#define MAX_SNAPS      4096
#define BEAM_WIDTH     12         /* states kept between iterations         */
#define MAX_CANDIDATES 6000
#define MIN_GAIN       10.0f      /* a jump must get us this much further   */
#define MAX_STEPS      60000000L
#define MAX_ROUNDS     400

typedef struct {
    float x, y, vy, rot;
    int   step;                /* physics steps executed so far          */
} Snapshot;

typedef struct {
    Player    p;
    int       step;            /* physics steps executed so far          */
    JumpPoint plan[MAX_PLAN];
    int       plan_len;
} Node;

typedef struct {
    Node  node;
    float reach;               /* how far a plain run from `node` gets    */
} Candidate;

static long g_budget;

typedef struct {
    float best;
    bool  finished;
} RunResult;

/* Runs forward from `start` without jumping, and records every grounded frame
 * into `snaps` (at most one every SNAP_STEP pixels). */
static RunResult run(const Level *l, Player start, int start_step,
                     Snapshot *snaps, int *snap_count, int snap_cap)
{
    RunResult res = { start.x, false };
    Player p = start;
    int step = start_step;

    while (g_budget > 0) {
        g_budget--;
        bool ok = player_step(l, &p, false, PHYS_DT);
        step++;                 /* `step` is now the next index to consume */
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
        /* only frames where a jump is actually possible are interesting */
        if (snaps && p.on_ground && *snap_count < snap_cap) {
            if (*snap_count == 0 ||
                p.x - snaps[*snap_count - 1].x >= SNAP_STEP) {
                Snapshot *s = &snaps[(*snap_count)++];
                s->x = p.x;
                s->y = p.y;
                s->vy = p.vy;
                s->rot = p.rot;
                s->step = step;
            }
        }
    }
    if (p.finished) {
        res.finished = true;
    }
    return res;
}

static Player player_at(const Snapshot *s)
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

static int cmp_candidate(const void *a, const void *b)
{
    const Candidate *ca = (const Candidate *)a;
    const Candidate *cb = (const Candidate *)b;
    if (ca->reach < cb->reach) return 1;
    if (ca->reach > cb->reach) return -1;
    return 0;
}

static void push_candidate(Candidate *cands, int *count, const Node *base,
                           JumpPoint jump, Player after_jump, float reach)
{
    if (*count >= MAX_CANDIDATES || base->plan_len >= MAX_PLAN) {
        return;
    }
    Candidate *c = &cands[(*count)++];
    c->node.p = after_jump;
    c->node.step = jump.step + 1;
    memcpy(c->node.plan, base->plan, sizeof(JumpPoint) * (size_t)base->plan_len);
    c->node.plan[base->plan_len] = jump;
    c->node.plan_len = base->plan_len + 1;
    c->reach = reach;
}

bool level_solve(const Level *l, JumpPoint *jump_x, int max_jumps, int *jump_count,
                 long *nodes, float *reach)
{
    Player start;
    player_reset(&start, l);

    g_budget = MAX_STEPS;

    Candidate *cands = (Candidate *)malloc(sizeof(Candidate) * MAX_CANDIDATES);
    Snapshot *snaps = (Snapshot *)malloc(sizeof(Snapshot) * MAX_SNAPS);
    Node *beam = (Node *)malloc(sizeof(Node) * BEAM_WIDTH);
    Node *next = (Node *)malloc(sizeof(Node) * BEAM_WIDTH);
    bool ok = false;

    if (!cands || !snaps || !beam || !next) {
        free(cands);
        free(snaps);
        free(beam);
        free(next);
        return false;
    }

    memset(&beam[0], 0, sizeof(Node));
    beam[0].p = start;
    beam[0].plan_len = 0;
    int beam_count = 1;
    int rounds = 0;

    while (beam_count > 0 && rounds++ < MAX_ROUNDS && g_budget > 0) {
        int cand_count = 0;

        for (int b = 0; b < beam_count; b++) {
            Node *base = &beam[b];
            int snap_count = 0;
            RunResult plain = run(l, base->p, base->step, snaps, &snap_count,
                                  MAX_SNAPS);

            if (plain.finished) {
                memcpy(jump_x, base->plan, sizeof(JumpPoint) * (size_t)base->plan_len);
                if (jump_count) *jump_count = base->plan_len;
                ok = true;
                goto done;
            }

            for (int i = 0; i < snap_count && g_budget > 0; i++) {
                Player jumper = player_at(&snaps[i]);
                JumpPoint jump = { snaps[i].step, snaps[i].x };
                int jump_step = snaps[i].step;
                bool alive = player_step(l, &jumper, true, PHYS_DT);
                if (!alive) {
                    continue;
                }
                if (jumper.finished) {
                    push_candidate(cands, &cand_count, base, jump, jumper,
                                   l->finish_x);
                    continue;
                }
                JumpPoint *unused = NULL;
                (void)unused;
                RunResult probe = run(l, jumper, jump_step + 1, NULL, NULL, 0);
                if (probe.finished) {
                    push_candidate(cands, &cand_count, base, jump, jumper,
                                   l->finish_x);
                    continue;
                }
                if (probe.best > plain.best + MIN_GAIN) {
                    push_candidate(cands, &cand_count, base, jump, jumper,
                                   probe.best);
                }
            }
        }

        if (cand_count == 0) {
            break;
        }

        qsort(cands, (size_t)cand_count, sizeof(Candidate), cmp_candidate);

        /* keep the best, but do not keep two states that start almost at the
         * same place - they lead to the same future */
        int kept = 0;
        for (int i = 0; i < cand_count && kept < BEAM_WIDTH; i++) {
            bool dup = false;
            for (int k = 0; k < kept; k++) {
                if (SDL_fabsf(cands[i].node.p.x - next[k].p.x) < 6.0f &&
                    SDL_fabsf(cands[i].node.p.y - next[k].p.y) < 6.0f) {
                    dup = true;
                    break;
                }
            }
            if (dup) {
                continue;
            }
            next[kept++] = cands[i].node;
            if (cands[i].reach >= l->finish_x) {
                if (getenv("GD_BOT_DEBUG")) {
                    fprintf(stderr, "success: plan_len=%d state x=%.1f step=%d\n",
                            cands[i].node.plan_len, cands[i].node.p.x,
                            cands[i].node.step);
                    for (int d = 0; d < cands[i].node.plan_len && d < 16; d++) {
                        fprintf(stderr, "  plan[%d] step=%d x=%.1f\n", d,
                                cands[i].node.plan[d].step, cands[i].node.plan[d].x);
                    }
                }
                memcpy(jump_x, cands[i].node.plan,
                       sizeof(JumpPoint) * (size_t)cands[i].node.plan_len);
                if (jump_count) *jump_count = cands[i].node.plan_len;
                ok = true;
                goto done;
            }
        }

        Node *tmp = beam;
        beam = next;
        next = tmp;
        beam_count = kept;

        if (getenv("GD_BOT_DEBUG")) {
            fprintf(stderr, "round %d: beam %d, best x=%.0f\n", rounds, beam_count,
                    kept ? beam[0].p.x : 0.0f);
        }
    }

done:
    if (ok) {
        /* Never trust the search: replay the plan with the real physics and
         * make sure it really reaches the finish line. */
        Player p;
        player_reset(&p, l);
        int idx = 0;
        int total = jump_count ? *jump_count : 0;
        bool finished = false;
        for (int step = 0; step < 2000000; step++) {
            bool hold = (idx < total && jump_x[idx].step == step);
            if (hold) {
                idx++;
            }
            bool alive = player_step(l, &p, hold, PHYS_DT);
            if (p.finished) {
                finished = true;
                break;
            }
            if (!alive) {
                break;
            }
        }
        if (!finished) {
            if (getenv("GD_BOT_DEBUG")) {
                fprintf(stderr, "replay check FAILED at x=%.1f (jump %d of %d)\n",
                        p.x, idx, total);
            }
            ok = false;
        } else if (getenv("GD_BOT_DEBUG")) {
            fprintf(stderr, "replay check OK\n");
        }
    }

    if (ok && jump_count && *jump_count > max_jumps) {
        /* the caller asked for a smaller plan than we found */
        ok = false;
    }

    if (nodes) {
        *nodes = MAX_STEPS - g_budget;
    }
    if (reach) {
        long saved = g_budget;
        g_budget = 4000000L;
        RunResult r = run(l, start, 0, NULL, NULL, 0);
        *reach = r.finished ? l->finish_x : r.best;
        g_budget = saved;
    }
    if (!ok && jump_count) {
        *jump_count = 0;
    }

    free(cands);
    free(snaps);
    free(beam);
    free(next);
    return ok;
}
