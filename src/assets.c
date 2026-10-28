/*
 * Texture loading.  Uses stb_image so that the only runtime dependency is SDL3.
 */
#include "gd.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include <stdio.h>
#include <string.h>

static SDL_Texture *load_from_memory(SDL_Renderer *ren, const void *data,
                                     size_t size, const char *name)
{
    int w = 0, h = 0, comp = 0;
    stbi_uc *pixels = stbi_load_from_memory((const stbi_uc *)data, (int)size,
                                            &w, &h, &comp, 4);
    if (!pixels) {
        SDL_Log("stb_image failed on %s: %s", name, stbi_failure_reason());
        return NULL;
    }

    SDL_Surface *surf = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32,
                                              pixels, w * 4);
    if (!surf) {
        stbi_image_free(pixels);
        return NULL;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_DestroySurface(surf);
    stbi_image_free(pixels);
    return tex;
}

SDL_Texture *texture_load(SDL_Renderer *ren, const char *relpath)
{
    const char *base = SDL_GetBasePath();
    char candidates[6][512];
    int n = 0;

    SDL_snprintf(candidates[n++], sizeof(candidates[0]), "%s", relpath);
    SDL_snprintf(candidates[n++], sizeof(candidates[0]), "assets/%s", relpath);
    if (base) {
        SDL_snprintf(candidates[n++], sizeof(candidates[0]), "%sassets/%s",
                     base, relpath);
        SDL_snprintf(candidates[n++], sizeof(candidates[0]), "%s%s", base, relpath);
        SDL_snprintf(candidates[n++], sizeof(candidates[0]), "%s../assets/%s",
                     base, relpath);
        SDL_snprintf(candidates[n++], sizeof(candidates[0]), "%s../../assets/%s",
                     base, relpath);
    }

    for (int i = 0; i < n; i++) {
        size_t size = 0;
        void *data = SDL_LoadFile(candidates[i], &size);
        if (!data) {
            continue;
        }
        SDL_Texture *tex = load_from_memory(ren, data, size, candidates[i]);
        SDL_free(data);
        if (tex) {
            SDL_Log("Loaded texture: %s", candidates[i]);
            return tex;
        }
    }
    SDL_Log("Could not load texture '%s'", relpath);
    return NULL;
}
