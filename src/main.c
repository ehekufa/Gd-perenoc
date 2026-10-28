/*
 * Point of No Return - entry point, window/event loop and the headless
 * self test modes used by CI.
 *
 * Build a desktop binary:   cmake -B build && cmake --build build
 * Run:                      ./build/point_of_no_return
 * Verify the level:         ./build/point_of_no_return --selftest
 */
#include "gd.h"

/* Mandatory on Android (and harmless everywhere else): SDL renames main() to
 * SDL_main() and exports it, which is the symbol SDLActivity looks up in
 * libmain.so.  Without this the native library loads but the activity finds
 * no entry point and the app closes right after its splash screen. */
#include <SDL3/SDL_main.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    bool selftest;
    bool autoplay;
    bool headless;
    bool no_autoplay;
    int  seconds;
    const char *screenshot;
    const char *level;
    int  shot_time_ms;
    int  width, height;
} Options;

static void parse_args(Options *o, int argc, char **argv)
{
    memset(o, 0, sizeof(*o));
    o->width = VIEW_W;
    o->height = VIEW_H;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest")) {
            o->selftest = o->headless = true;
        } else if (!strcmp(argv[i], "--autoplay")) {
            o->autoplay = true;
        } else if (!strcmp(argv[i], "--level") && i + 1 < argc) {
            o->level = argv[++i];
        } else if (!strcmp(argv[i], "--manual")) {
            o->no_autoplay = true;
        } else if (!strcmp(argv[i], "--headless")) {
            o->headless = true;
        } else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) {
            o->seconds = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) {
            o->screenshot = argv[++i];
            o->headless = true;
        } else if (!strcmp(argv[i], "--shot-time") && i + 1 < argc) {
            o->shot_time_ms = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--size") && i + 1 < argc) {
            o->width = atoi(argv[++i]);
            o->height = atoi(argv[++i]);
            if (o->width <= 0) o->width = VIEW_W;
            if (o->height <= 0) o->height = VIEW_H;
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("Point of No Return - a Geometry Dash style game\n\n"
                   "  --selftest            verify the level with the autopilot\n"
                   "  --autoplay            let the autopilot play\n"
                   "  --manual              do not autoplay in --screenshot mode\n"
                   "  --level FILE.lvl      play a custom level\n"
                   "  --headless            no window (needs --screenshot)\n"
                   "  --screenshot FILE     render one frame to FILE.bmp\n"
                   "  --shot-time MS        frame time for --screenshot\n"
                   "  --size W H            window size\n");
            exit(0);
        }
    }
}

/* Checks the .lvl parser on tiny inline levels. */
static int selftest_parser(void)
{
    Level l;
    int failures = 0;

    if (!level_load_from_text(&l,
            "!name TEST LEVEL\n"
            ".2.\n"
            "111\n", "inline")) {
        printf("parser: simple level failed to load\n");
        return 1;
    }
    if (l.count != 2) {
        printf("parser: expected 2 objects, got %d\n", l.count);
        failures++;
    }
    if (l.count == 2) {
        /* one merged block 3 cells wide + one spike */
        bool block = l.objs[0].kind == OBJ_BLOCK && l.objs[0].w == 3 * CELL;
        bool spike = l.objs[1].kind == OBJ_SPIKE && l.objs[1].x == CELL;
        if (!block || !spike) {
            printf("parser: wrong object geometry\n");
            failures++;
        }
    }
    if (SDL_strcmp(l.name, "TEST LEVEL") != 0) {
        printf("parser: !name directive ignored (%s)\n", l.name);
        failures++;
    }
    level_free(&l);

    /* a tall wall must become one block object, not a stack of cells */
    if (!level_load_from_text(&l, "1\n1\n1\n", "wall")) {
        printf("parser: vertical level failed to load\n");
        return 1;
    }
    if (l.count != 1 || l.objs[0].h != 3 * CELL) {
        printf("parser: tall wall was not merged (%d objects)\n", l.count);
        failures++;
    }
    level_free(&l);

    /* unknown characters must be rejected */
    if (level_load_from_text(&l, "1Z1\n", "bad")) {
        printf("parser: unknown character was accepted\n");
        failures++;
        level_free(&l);
    }

    if (failures == 0) {
        printf("parser: OK\n");
    }
    return failures;
}

static int run_selftest(const Options *o)
{
    Level level;

    int parser_failures = selftest_parser();
    if (parser_failures) {
        printf("SELFTEST FAILED: %d parser problem(s)\n", parser_failures);
        return 1;
    }

    const char *path = o->level ? o->level : DEFAULT_LEVEL;
    if (!level_load(&level, path)) {
        printf("could not load level '%s', testing the built in fallback\n", path);
        level_build(&level);
    }

    printf("level: " LEVEL_NAME " (%.1f blocks, %.1f s)\n",
           level_length_blocks(&level), level_length_blocks(&level) * CELL / RUN_SPEED);

    JumpPoint jumps[MAX_AUTO_JUMPS];
    int count = 0;
    long nodes = 0;
    float reach = 0.0f;
    bool ok = level_solve(&level, jumps, MAX_AUTO_JUMPS, &count, &nodes, &reach);

    printf("objects: %d, autopilot: %s, jumps: %d, sim steps: %ld, reach: %.0f px\n",
           level.count, ok ? "solved" : "FAILED", count, nodes, reach);

    if (!ok) {
        level_free(&level);
        printf("SELFTEST FAILED: the level cannot be completed\n");
        return 1;
    }

    /* Replay the recorded jumps with the real game loop and make sure the
     * cube actually reaches the finish line. */
    Game g;
    if (!game_init(&g, true, o->width, o->height, o->level)) {
        printf("could not initialise the game: %s\n", SDL_GetError());
        level_free(&level);
        return 1;
    }
    g.autoplay = true;
    g.attempts = 1;
    game_compute_autopilot(&g);

    float t = 0.0f;
    float dt = 1.0f / 60.0f;
    while (g.state == STATE_PLAY || g.state == STATE_DYING) {
        game_update(&g, false, dt);
        t += dt;
        if (t > 120.0f) {
            break;
        }
    }

    bool won = (g.state == STATE_WIN);
    printf("replay: %s in %.2f s (%.0f fps of simulated time)\n",
           won ? "COMPLETED" : "did not finish", t, 1.0f / dt);
    printf("attempts used: %d, deaths: %d\n", g.attempts, g.deaths);

    game_shutdown(&g);
    level_free(&level);
    if (!won) {
        printf("SELFTEST FAILED: the autopilot could not finish the level\n");
        return 1;
    }
    printf("SELFTEST OK\n");
    return 0;
}

static int run_screenshot(const Options *o)
{
    Game g;
    if (!game_init(&g, true, o->width, o->height, o->level)) {
        printf("could not initialise the game: %s\n", SDL_GetError());
        return 1;
    }
    g.autoplay = !o->no_autoplay;
    g.attempts = 1;
    if (g.autoplay) {
        game_compute_autopilot(&g);
    }

    float dt = 1.0f / 60.0f;
    float target = (float)o->shot_time_ms / 1000.0f;
    for (float t = 0.0f; t < target; t += dt) {
        game_update(&g, false, dt);
    }
    game_screenshot(&g, o->screenshot);
    printf("screenshot: %s at %.2f s, state=%d, x=%.0f\n", o->screenshot,
           target, g.state, g.player.x);
    game_shutdown(&g);
    return 0;
}

int main(int argc, char **argv)
{
    Options opt;
    parse_args(&opt, argc, argv);

    if (opt.headless) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    }

    /* Video is the only subsystem the game cannot live without.  Audio and
     * gamepads are optional: on a phone a missing audio driver must not stop
     * the game from starting (that is what used to make it quit on launch). */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError());
        return 1;
    }
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("Audio is not available, continuing without sound: %s",
                SDL_GetError());
    }
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        SDL_Log("Gamepad support is not available: %s", SDL_GetError());
    }

    SDL_Log("Point of No Return: video driver = %s, audio driver = %s",
            SDL_GetCurrentVideoDriver(),
            SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "none");

    if (opt.selftest) {
        int rc = run_selftest(&opt);
        SDL_Quit();
        return rc;
    }
    if (opt.screenshot) {
        int rc = run_screenshot(&opt);
        SDL_Quit();
        return rc;
    }

    Game g;
    if (!game_init(&g, false, opt.width, opt.height, opt.level)) {
        fprintf(stderr, "could not start the game: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    g.autoplay = opt.autoplay;
    g.attempts = 1;

    SDL_Log("Point of No Return started (%s)", SDL_GetCurrentVideoDriver());

    const bool *keys = SDL_GetKeyboardState(NULL);
    Uint64 prev = SDL_GetPerformanceCounter();
    bool running = true;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.key == SDLK_ESCAPE) {
                    if (g.state == STATE_PLAY) {
                        g.paused = !g.paused;
                    }
                } else if (ev.key.key == SDLK_R && !ev.key.repeat) {
                    game_respawn(&g);
                } else if (ev.key.key == SDLK_F1 && !ev.key.repeat) {
                    g.autoplay = !g.autoplay;
                    g.auto_jump_index = 0;
                    if (g.autoplay) {
                        game_compute_autopilot(&g);
                    }
                } else if (ev.key.key == SDLK_F2 && !ev.key.repeat) {
                    game_screenshot(&g, "screenshot.bmp");
                } else if (ev.key.key == SDLK_SPACE && g.state == STATE_WIN) {
                    game_respawn(&g);
                } else if (ev.key.key == SDLK_ESCAPE && g.paused) {
                    g.paused = false;
                }
                break;
            case SDL_EVENT_FINGER_DOWN:
                g.touches_down++;
                if (g.state == STATE_WIN) {
                    game_respawn(&g);
                } else if (g.paused) {
                    g.paused = false;
                }
                break;
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_CANCELED:
                if (g.touches_down > 0) {
                    g.touches_down--;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                if (g.state == STATE_WIN) {
                    game_respawn(&g);
                } else if (g.paused) {
                    g.paused = false;
                }
                break;
            }
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                if (g.state == STATE_PLAY) {
                    g.paused = true;
                }
                break;
            default:
                break;
            }
        }

        bool hold = keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_UP] ||
                    keys[SDL_SCANCODE_W];

        /* touch / mouse held down also counts as jump */
        Uint32 buttons = SDL_GetMouseState(NULL, NULL);
        if (buttons & SDL_BUTTON_LMASK) {
            hold = true;
        }
        if (g.touches_down > 0) {
            hold = true;
        }
        {
            int pads = 0;
            SDL_JoystickID *ids = SDL_GetGamepads(&pads);
            for (int i = 0; i < pads; i++) {
                if (SDL_GetGamepadButton(SDL_GetGamepadFromID(ids[i]),
                                         SDL_GAMEPAD_BUTTON_SOUTH)) {
                    hold = true;
                }
            }
            SDL_free(ids);
        }

        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)((double)(now - prev) / (double)SDL_GetPerformanceFrequency());
        prev = now;
        if (dt > 0.1f) {
            dt = 0.1f;
        }

        game_update(&g, hold, dt);
        game_render(&g);
    }

    game_shutdown(&g);
    SDL_Quit();
    return 0;
}
