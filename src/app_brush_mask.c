#include "app_brush_mask.h"

#include <stdlib.h>

int app_brush_mask_contains(BrushShape shape, int x, int y, int radius) {
    switch (shape) {
    case BRUSH_SHAPE_ROUND:
        return x * x + y * y <= radius * radius;
    case BRUSH_SHAPE_SQUARE:
        return abs(x) <= radius && abs(y) <= radius;
    case BRUSH_SHAPE_DIAMOND:
        return abs(x) + abs(y) <= radius;
    default:
        return 0;
    }
}
