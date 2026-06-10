#include "font.h"
#include "font_data.h"

static int glyph_index(unsigned char ch) {
    if (ch < FONT_FIRST_CHAR || ch > FONT_LAST_CHAR) {
        ch = '?';
    }
    return ch - FONT_FIRST_CHAR;
}

static int glyph_advance(unsigned char ch) {
    return font_advances[glyph_index(ch)] + 1;
}

int font_text_width(const char *text, int scale) {
    int width = 0;
    int line_width = 0;

    if (!text || scale < 1) {
        return 0;
    }
    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            if (line_width > width) {
                width = line_width;
            }
            line_width = 0;
            continue;
        }
        line_width += glyph_advance((unsigned char)*p) * scale;
    }
    return line_width > width ? line_width : width;
}

int font_text_height(int scale) {
    return scale < 1 ? 0 : FONT_GLYPH_HEIGHT * scale;
}

void canvas_draw_text(Canvas *c, int x, int y, const char *text, int scale, uint32_t argb) {
    int pen_x = x;

    if (!c || !c->pixels || !text || scale < 1) {
        return;
    }
    for (const char *p = text; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\n') {
            pen_x = x;
            y += FONT_GLYPH_HEIGHT * scale;
            continue;
        }
        {
            const uint16_t *glyph = font_glyphs[glyph_index(ch)];
            for (int gy = 0; gy < FONT_GLYPH_HEIGHT; gy++) {
                uint16_t bits = glyph[gy];
                for (int gx = 0; gx < FONT_GLYPH_CELL_WIDTH; gx++) {
                    if (!(bits & (1 << gx))) {
                        continue;
                    }
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            canvas_set_pixel(c, pen_x + gx * scale + sx, y + gy * scale + sy, argb);
                        }
                    }
                }
            }
        }
        pen_x += glyph_advance(ch) * scale;
    }
}
