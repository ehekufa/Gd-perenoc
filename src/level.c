/*
 * Level loading.
 *
 * Levels are plain text files (".lvl").  One character is one grid cell:
 *
 *     .   empty
 *     1   block
 *     2   spike
 *
 * The first grid line is the top row, the last one rests on the ground.
 * Lines starting with '#' or ';' are comments, and a few directives are
 * supported at the top of the file:
 *
 *     !name  POINT OF NO RETURN
 *     !spawn 6
 *
 * See assets/levels/point_of_no_return.lvl for the first level.
 */
#include "gd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEVEL_MAX_ROWS   96
#define LEVEL_MAX_COLS   4096

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

/* Block occupying `w` cells horizontally and `h` cells vertically, with its
 * bottom row at `row` (row 0 is the row that stands on the ground). */
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

static int cell_from_char(char c)
{
    switch (c) {
    case '.': case '0': case ' ': case '_':
        return 0;
    case '1': case 'X': case 'x': case 'B': case 'b':
        return 1;
    case '2': case '^': case 'S': case 's':
        return 2;
    default:
        return -1;
    }
}

static bool parse_grid(Level *l, char **rows, int row_count, int width,
                       int *error_line, int *error_col)
{
    /* Merge runs of block cells into as few objects as possible: a wall of
     * five cells becomes one object, not five.  We walk top to bottom and
     * grow each newly found block run downwards and to the right. */
    for (int r = 0; r < row_count; r++) {
        for (int c = 0; c < width; c++) {
            if (cell_from_char(rows[r][c]) != 1) {
                continue;
            }
            /* how wide is this run on the current row? */
            int run = 1;
            while (c + run < width && cell_from_char(rows[r][c + run]) == 1) {
                run++;
            }
            /* grow downwards while the whole run is still made of blocks */
            int height = 1;
            while (r + height < row_count) {
                bool same = true;
                for (int i = 0; i < run; i++) {
                    if (cell_from_char(rows[r + height][c + i]) != 1) {
                        same = false;
                        break;
                    }
                }
                if (!same) {
                    break;
                }
                height++;
            }
            /* bottom row of the level is row index `row_count - 1` */
            int bottom_row = (row_count - 1) - (r + height - 1);
            L_block(l, c, bottom_row, run, height);
            for (int cy = 0; cy < height; cy++) {
                for (int cx = 0; cx < run; cx++) {
                    rows[r + cy][c + cx] = ' ';
                }
            }
            c += run - 1;
        }
    }

    for (int r = 0; r < row_count; r++) {
        for (int c = 0; c < width; c++) {
            int kind = cell_from_char(rows[r][c]);
            if (kind == -1) {
                if (error_line) *error_line = r + 1;
                if (error_col) *error_col = c + 1;
                return false;
            }
            if (kind == 2) {
                int bottom_row = (row_count - 1) - r;
                L_spike(l, c, bottom_row);
            }
        }
    }
    return true;
}

bool level_load_from_text(Level *l, const char *text, const char *label)
{
    memset(l, 0, sizeof(*l));
    SDL_strlcpy(l->name, LEVEL_NAME, sizeof(l->name));
    l->spawn_col = 6;

    char *copy = SDL_strdup(text);
    if (!copy) {
        return false;
    }

    char *rows[LEVEL_MAX_ROWS];
    int row_count = 0;
    int width = 0;
    bool ok = true;

    char *cursor = copy;
    int line_no = 0;
    while (cursor && *cursor) {
        char *line = cursor;
        char *nl = strchr(cursor, '\n');
        if (nl) {
            *nl = '\0';
            cursor = nl + 1;
        } else {
            cursor = NULL;
        }
        line_no++;

        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ' ||
                           line[len - 1] == '\t')) {
            line[--len] = '\0';
        }
        if (len == 0 || line[0] == '#' || line[0] == ';') {
            continue;
        }

        if (line[0] == '!') {
            char *arg = line + 1;
            while (*arg == ' ' || *arg == '\t') {
                arg++;
            }
            if (!SDL_strncasecmp(arg, "name", 4)) {
                arg += 4;
                while (*arg == ' ' || *arg == '\t' || *arg == ':') {
                    arg++;
                }
                SDL_strlcpy(l->name, arg, sizeof(l->name));
            } else if (!SDL_strncasecmp(arg, "spawn", 5)) {
                l->spawn_col = atoi(arg + 5);
                if (l->spawn_col < 0) {
                    l->spawn_col = 0;
                }
            }
            continue;
        }

        if (row_count >= LEVEL_MAX_ROWS) {
            SDL_Log("Level '%s' has more than %d rows", label, LEVEL_MAX_ROWS);
            ok = false;
            break;
        }
        rows[row_count++] = line;
        if ((int)len > width) {
            width = (int)len;
        }
        if (width > LEVEL_MAX_COLS) {
            SDL_Log("Level '%s' is wider than %d cells", label, LEVEL_MAX_COLS);
            ok = false;
            break;
        }
    }

    if (ok && row_count == 0) {
        SDL_Log("Level '%s' has no grid", label);
        ok = false;
    }

    if (ok) {
        int err_line = 0, err_col = 0;
        ok = parse_grid(l, rows, row_count, width, &err_line, &err_col);
        if (!ok) {
            SDL_Log("Level '%s': unknown character at line %d, column %d "
                    "(use '.', '1' or '2')", label, err_line, err_col);
            l->count = 0;
        }
    }

    SDL_free(copy);

    if (!ok) {
        return false;
    }

    l->finish_x = (float)width * CELL;
    SDL_Log("Level '%s': %d x %d cells, %d objects", l->name, width, row_count,
            l->count);
    return true;
}

bool level_load(Level *l, const char *relpath)
{
    size_t size = 0;
    char *text = (char *)asset_load(relpath, &size);
    if (!text) {
        SDL_Log("Could not open level '%s'", relpath);
        return false;
    }
    bool ok = level_load_from_text(l, text, relpath);
    SDL_free(text);
    return ok;
}

/* Used only when the .lvl file cannot be found (e.g. a stripped build). */
void level_build(Level *l)
{
    memset(l, 0, sizeof(*l));
    SDL_strlcpy(l->name, LEVEL_NAME, sizeof(l->name));
    l->spawn_col = 6;
    L_spike(l, 13, 0);
    L_spike(l, 19, 0);
    L_block(l, 38, 0, 3, 1);
    L_spike(l, 43, 0);
    l->finish_x = 60.0f * CELL;
}
