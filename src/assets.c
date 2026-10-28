/*
 * Asset loading.  Textures go through stb_image, so the only runtime
 * dependency is SDL3.  Everything is looked up relative to the working
 * directory, the executable and (on Android) the APK assets.
 */
#include "gd.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include <stdio.h>
#include <string.h>

static bool file_exists(const char *path)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "rb");
    if (!io) {
        return false;
    }
    SDL_CloseIO(io);
    return true;
}

const char *asset_find(const char *relpath, char *buffer, size_t buffer_size)
{
    const char *base = SDL_GetBasePath();

    for (int attempt = 0; attempt < 5; attempt++) {
        switch (attempt) {
        case 0:
            SDL_snprintf(buffer, buffer_size, "assets/%s", relpath);
            break;
        case 1:
            SDL_snprintf(buffer, buffer_size, "../assets/%s", relpath);
            break;
        case 2:
            SDL_snprintf(buffer, buffer_size, "../../assets/%s", relpath);
            break;
        case 3:
            if (!base) {
                continue;
            }
            SDL_snprintf(buffer, buffer_size, "%sassets/%s", base, relpath);
            break;
        default:
            /* inside the APK the assets are addressed by their plain name */
            SDL_snprintf(buffer, buffer_size, "%s", relpath);
            break;
        }
        if (file_exists(buffer)) {
            return buffer;
        }
    }
    return NULL;
}

void *asset_load(const char *relpath, size_t *size)
{
    char path[512];
    const char *found = asset_find(relpath, path, sizeof(path));
    if (!found) {
        SDL_Log("Asset not found: %s", relpath);
        return NULL;
    }
    void *data = SDL_LoadFile(found, size);
    if (!data) {
        SDL_Log("Could not read asset %s: %s", found, SDL_GetError());
    }
    return data;
}

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
    size_t size = 0;
    void *data = asset_load(relpath, &size);
    if (!data) {
        return NULL;
    }
    SDL_Texture *tex = load_from_memory(ren, data, size, relpath);
    SDL_free(data);
    if (tex) {
        SDL_Log("Loaded texture: %s", relpath);
    }
    return tex;
}

SDL_Texture *texture_load_optional(SDL_Renderer *ren, const char *relpath)
{
    char path[512];
    if (!asset_find(relpath, path, sizeof(path))) {
        return NULL;
    }
    return texture_load(ren, relpath);
}
