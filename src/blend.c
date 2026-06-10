#include "blend.h"

#include <math.h>

const char *blend_mode_name(BlendMode mode) {
    switch (mode) {
    case BLEND_NORMAL: return "Normal";
    case BLEND_MULTIPLY: return "Multiply";
    case BLEND_SCREEN: return "Screen";
    case BLEND_OVERLAY: return "Overlay";
    case BLEND_SOFT_LIGHT: return "Soft Light";
    case BLEND_HARD_LIGHT: return "Hard Light";
    case BLEND_DARKEN: return "Darken";
    case BLEND_LIGHTEN: return "Lighten";
    case BLEND_COLOR_DODGE: return "Color Dodge";
    case BLEND_COLOR_BURN: return "Color Burn";
    case BLEND_LINEAR_DODGE: return "Linear Dodge";
    case BLEND_LINEAR_BURN: return "Linear Burn";
    case BLEND_DIFFERENCE: return "Difference";
    case BLEND_EXCLUSION: return "Exclusion";
    default: return "Normal";
    }
}

int blend_mode_valid(int mode) {
    return mode >= 0 && mode < BLEND_MODE_COUNT;
}

BlendMode blend_mode_cycle(BlendMode mode, int direction) {
    int next = (int)mode + (direction >= 0 ? 1 : -1);
    if (next < 0) {
        next = BLEND_MODE_COUNT - 1;
    }
    if (next >= BLEND_MODE_COUNT) {
        next = 0;
    }
    return (BlendMode)next;
}

static uint8_t clamp_u8(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

static uint8_t soft_light_channel(uint8_t dst, uint8_t src) {
    double d = dst / 255.0;
    double s = src / 255.0;
    double out;

    if (s <= 0.5) {
        out = d - (1.0 - 2.0 * s) * d * (1.0 - d);
    } else {
        double dd = d <= 0.25 ? ((16.0 * d - 12.0) * d + 4.0) * d : sqrt(d);
        out = d + (2.0 * s - 1.0) * (dd - d);
    }
    return clamp_u8((int)(out * 255.0 + 0.5));
}

uint8_t blend_mode_channel(BlendMode mode, uint8_t dst, uint8_t src) {
    int d = dst;
    int s = src;

    switch (mode) {
    case BLEND_MULTIPLY:
        return (uint8_t)((d * s + 127) / 255);
    case BLEND_SCREEN:
        return (uint8_t)(255 - ((255 - d) * (255 - s) + 127) / 255);
    case BLEND_OVERLAY:
        return blend_mode_channel(BLEND_HARD_LIGHT, src, dst);
    case BLEND_SOFT_LIGHT:
        return soft_light_channel(dst, src);
    case BLEND_HARD_LIGHT:
        if (s <= 127) {
            return (uint8_t)((d * 2 * s + 127) / 255);
        }
        return (uint8_t)(255 - ((255 - d) * (2 * (255 - s)) + 127) / 255);
    case BLEND_DARKEN:
        return (uint8_t)(d < s ? d : s);
    case BLEND_LIGHTEN:
        return (uint8_t)(d > s ? d : s);
    case BLEND_COLOR_DODGE:
        if (s >= 255) {
            return 255;
        }
        return clamp_u8((d * 255) / (255 - s));
    case BLEND_COLOR_BURN:
        if (s <= 0) {
            return 0;
        }
        return clamp_u8(255 - ((255 - d) * 255) / s);
    case BLEND_LINEAR_DODGE:
        return clamp_u8(d + s);
    case BLEND_LINEAR_BURN:
        return clamp_u8(d + s - 255);
    case BLEND_DIFFERENCE:
        return (uint8_t)(d > s ? d - s : s - d);
    case BLEND_EXCLUSION:
        return (uint8_t)(d + s - (2 * d * s + 127) / 255);
    case BLEND_NORMAL:
    default:
        return src;
    }
}

uint32_t blend_mode_composite(uint32_t dst, uint32_t src, BlendMode mode) {
    uint8_t sa = (uint8_t)((src >> 24) & 0xFF);
    uint8_t da = (uint8_t)((dst >> 24) & 0xFF);

    if (sa == 0) {
        return dst;
    }

    uint8_t sr = (uint8_t)((src >> 16) & 0xFF);
    uint8_t sg = (uint8_t)((src >> 8) & 0xFF);
    uint8_t sb = (uint8_t)(src & 0xFF);
    uint8_t dr = (uint8_t)((dst >> 16) & 0xFF);
    uint8_t dg = (uint8_t)((dst >> 8) & 0xFF);
    uint8_t db = (uint8_t)(dst & 0xFF);

    uint8_t br = sr;
    uint8_t bg = sg;
    uint8_t bb = sb;
    if (mode != BLEND_NORMAL && da > 0) {
        uint8_t mr = blend_mode_channel(mode, dr, sr);
        uint8_t mg = blend_mode_channel(mode, dg, sg);
        uint8_t mb = blend_mode_channel(mode, db, sb);
        br = (uint8_t)((mr * da + sr * (255 - da) + 127) / 255);
        bg = (uint8_t)((mg * da + sg * (255 - da) + 127) / 255);
        bb = (uint8_t)((mb * da + sb * (255 - da) + 127) / 255);
    }

    if (sa == 255) {
        return 0xFF000000u | ((uint32_t)br << 16) | ((uint32_t)bg << 8) | bb;
    }

    int inv = 255 - sa;
    uint8_t out_r = (uint8_t)((br * sa + dr * inv + 127) / 255);
    uint8_t out_g = (uint8_t)((bg * sa + dg * inv + 127) / 255);
    uint8_t out_b = (uint8_t)((bb * sa + db * inv + 127) / 255);
    uint8_t out_a = (uint8_t)(sa + ((da * inv + 127) / 255));

    return ((uint32_t)out_a << 24) | ((uint32_t)out_r << 16) | ((uint32_t)out_g << 8) | out_b;
}
