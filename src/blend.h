#ifndef BLEND_H
#define BLEND_H

#include <stdint.h>

typedef enum {
    BLEND_NORMAL = 0,
    BLEND_MULTIPLY,
    BLEND_SCREEN,
    BLEND_OVERLAY,
    BLEND_SOFT_LIGHT,
    BLEND_HARD_LIGHT,
    BLEND_DARKEN,
    BLEND_LIGHTEN,
    BLEND_COLOR_DODGE,
    BLEND_COLOR_BURN,
    BLEND_LINEAR_DODGE,
    BLEND_LINEAR_BURN,
    BLEND_DIFFERENCE,
    BLEND_EXCLUSION,
    BLEND_MODE_COUNT
} BlendMode;

const char *blend_mode_name(BlendMode mode);
int blend_mode_valid(int mode);
BlendMode blend_mode_cycle(BlendMode mode, int direction);
uint8_t blend_mode_channel(BlendMode mode, uint8_t dst, uint8_t src);
uint32_t blend_mode_composite(uint32_t dst, uint32_t src, BlendMode mode);

#endif
