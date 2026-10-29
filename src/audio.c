/*
 * Tiny synthesizer: every sound effect is generated into the audio stream on
 * the fly, so the game ships without any audio files.
 */
#include "gd.h"
#include <math.h>
#include <string.h>

#define SAMPLE_RATE 44100
#define MAX_VOICES  8

typedef struct {
    bool  active;
    int   id;
    float t;
    float dur;
    float phase;
} Voice;

static Voice g_voices[MAX_VOICES];

void audio_init(Game *g)
{
    memset(g_voices, 0, sizeof(g_voices));

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;
    spec.freq = SAMPLE_RATE;

    g->audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!g->audio_stream) {
        SDL_Log("Audio disabled: %s", SDL_GetError());
        g->audio_ok = false;
        return;
    }
    g->audio_ok = true;
    SDL_ResumeAudioStreamDevice(g->audio_stream);
}

void audio_shutdown(Game *g)
{
    if (g->audio_stream) {
        SDL_DestroyAudioStream(g->audio_stream);
        g->audio_stream = NULL;
    }
    g->audio_ok = false;
}

void audio_play(Game *g, int id)
{
    if (!g->audio_ok) {
        return;
    }
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!g_voices[i].active) {
            g_voices[i].active = true;
            g_voices[i].id = id;
            g_voices[i].t = 0.0f;
            g_voices[i].phase = 0.0f;
            switch (id) {
            case SFX_JUMP:  g_voices[i].dur = 0.09f; break;
            case SFX_BLOCK: g_voices[i].dur = 0.05f; break;
            case SFX_DEATH: g_voices[i].dur = 0.45f; break;
            case SFX_WIN:   g_voices[i].dur = 0.90f; break;
            default:        g_voices[i].dur = 0.15f; break;
            }
            return;
        }
    }
}

/* Called once per frame: renders a few milliseconds of audio into the stream. */
void audio_update(Game *g, float dt)
{
    if (!g->audio_ok) {
        return;
    }
    int frames = (int)(dt * SAMPLE_RATE);
    if (frames <= 0) {
        return;
    }
    if (frames > 2048) {
        frames = 2048;
    }

    static float buffer[2048];
    memset(buffer, 0, sizeof(float) * (size_t)frames);

    for (int v = 0; v < MAX_VOICES; v++) {
        Voice *vo = &g_voices[v];
        if (!vo->active) {
            continue;
        }
        for (int i = 0; i < frames; i++) {
            float local = vo->t + (float)i / SAMPLE_RATE;
            if (local >= vo->dur) {
                break;
            }
            float u = local / vo->dur;
            float env = (1.0f - u) * (1.0f - u);
            float s = 0.0f;

            switch (vo->id) {
            case SFX_JUMP:
                /* quick upward blip */
                s = sinf(6.2831853f * (520.0f + 700.0f * u) *
                         (float)i / SAMPLE_RATE) * 0.30f;
                break;
            case SFX_BLOCK:
                s = sinf(6.2831853f * 220.0f * (float)i / SAMPLE_RATE) * 0.15f;
                break;
            case SFX_DEATH:
                /* noisy falling buzz */
                s = ((float)(rng_next() & 0xFFFF) / 32768.0f - 1.0f) * 0.35f +
                    sinf(6.2831853f * (300.0f - 220.0f * u) *
                         (float)i / SAMPLE_RATE) * 0.25f;
                break;
            case SFX_WIN: {
                static const float notes[4] = { 523.25f, 659.25f, 783.99f, 1046.5f };
                int step = (int)(u * 4.0f);
                if (step > 3) step = 3;
                s = sinf(6.2831853f * notes[step] * (float)i / SAMPLE_RATE) * 0.30f;
                break;
            }
            default:
                s = sinf(6.2831853f * 440.0f * (float)i / SAMPLE_RATE) * 0.2f;
                break;
            }
            buffer[i] += s * env;
        }
        vo->t += dt;
        if (vo->t >= vo->dur) {
            vo->active = false;
        }
    }

    for (int i = 0; i < frames; i++) {
        if (buffer[i] > 1.0f) buffer[i] = 1.0f;
        if (buffer[i] < -1.0f) buffer[i] = -1.0f;
    }
    SDL_PutAudioStreamData(g->audio_stream, buffer, frames * (int)sizeof(float));
}
