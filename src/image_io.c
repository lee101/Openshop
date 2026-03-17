#include "image_io.h"

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
