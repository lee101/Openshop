#include "image_io.h"
#include "path_routing.h"

#include <SDL2/SDL.h>
#include <string.h>

int canvas_load_bmp(Canvas *c, const char *path, uint32_t background_color) {
    if (!path || !c) {
        return 0;
    }

    SDL_Surface *bmp = SDL_LoadBMP(path);
    if (!bmp) {
        return 0;
    }

    SDL_Surface *converted = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(bmp);
    if (!converted) {
        return 0;
    }

    canvas_clear(c, background_color);

    int copy_w = converted->w < c->width ? converted->w : c->width;
    int copy_h = converted->h < c->height ? converted->h : c->height;
    for (int y = 0; y < copy_h; y++) {
        uint8_t *row = (uint8_t *)converted->pixels + y * converted->pitch;
        memcpy(c->pixels + y * c->width, row, (size_t)copy_w * sizeof(uint32_t));
    }

    SDL_FreeSurface(converted);
    return 1;
}

int canvas_save_bmp(const Canvas *c, const char *path) {
    if (!c || !c->pixels || !path) {
        return 0;
    }

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        (void *)c->pixels,
        c->width,
        c->height,
        32,
        c->width * 4,
        SDL_PIXELFORMAT_ARGB8888
    );
    if (!surface) {
        return 0;
    }

    int ok = SDL_SaveBMP(surface, path) == 0;
    SDL_FreeSurface(surface);
    return ok;
}

int canvas_load_auto(Canvas *c, const char *path, uint32_t background_color) {
    if (!c || !path || !path[0]) {
        return 0;
    }
    if (path_has_extension_ci(path, ".png")) {
        return canvas_load_png(c, path, background_color);
    }
    if (path_has_extension_ci(path, ".bmp")) {
        return canvas_load_bmp(c, path, background_color);
    }
    if (canvas_load_png(c, path, background_color)) {
        return 1;
    }
    return canvas_load_bmp(c, path, background_color);
}

int canvas_save_auto(const Canvas *c, const char *path) {
    if (!c || !path || !path[0]) {
        return 0;
    }
    if (path_has_extension_ci(path, ".png")) {
        return canvas_save_png(c, path);
    }
    return canvas_save_bmp(c, path);
}
