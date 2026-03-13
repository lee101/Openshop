# Openshop
Photoshop alternative — minimal C + SDL2 prototype.

## Build
Requires SDL2 development packages (`libsdl2-dev` on Debian/Ubuntu).

```bash
make
```

## Test (no SDL2 required)
```bash
make test
```

## Run
```bash
./openshop [optional_input.bmp]
```

---

## Controls

### Drawing tools
| Key | Tool |
|-----|------|
| `B` | Hard brush (circle, Bresenham stroke) |
| `Q` | Soft brush (gaussian / SDF falloff — smooth edges) |
| `A` | Airbrush / spray (random scatter within radius) |
| `E` | Eraser |
| `L` | Line |
| `R` | Rectangle outline |
| `T` | Filled rectangle |
| `O` | Ellipse outline |
| `P` | Filled ellipse |

### Painting
| Key | Action |
|-----|--------|
| `Left mouse` | Draw / place shape |
| `Right mouse` / `I` | Eyedropper — pick colour |
| `Shift` (held with shape tool) | Constrain to square / circle / 45° |
| `[ / ]` | Brush size −/+ (1–128) |
| `- / =` | Opacity −5% / +5% (1–100%) |
| `F` | Flood fill at cursor |
| `C` | Clear canvas |

### Quick palette (1–9)
`1` black · `2` red · `3` green · `4` blue ·
`5` yellow · `6` purple · `7` orange · `8` cyan · `9` white

### File
| Key | Action |
|-----|--------|
| `Ctrl+S` | Save → `output.bmp` |
| `Ctrl+O` | Load ← `input.bmp` |

### Undo / Redo
`Ctrl+Z` undo · `Ctrl+Y` redo  (up to 20 levels)

### Filters / effects (all undoable)
| Key | Effect |
|-----|--------|
| `Ctrl+G` | 5×5 Gaussian blur |
| `Ctrl+J` | 3×3 Box blur |
| `Ctrl+H` | Sharpen (unsharp 3×3) |
| `Ctrl+M` | Emboss / relief |
| `Ctrl+N` | Edge detect (Sobel) |
| `Ctrl+I` | Invert colours |
| `Ctrl+D` | Grayscale (BT.601 luminosity) |
| `Ctrl+V` | Vignette (darken corners) |
| `Ctrl+K` | Pixelate ×8 |
| `Ctrl+U` | Posterize (4 levels) |

### Misc
`Esc` — quit

---

## Architecture

```
src/
  main.c      entry point
  app.c       SDL2 event loop, tools, shortcuts
  canvas.c    pixel engine: hard/soft/spray brushes, primitives, flood fill
  canvas.h
  filters.c   CPU convolution kernels + image effects
  filters.h
tests/
  canvas_smoke.c    unit tests (no SDL2)
  image_selftest.c  deterministic BMP rendering tests
```

### Brush types
- **Hard brush** — filled disc, Bresenham interpolation between samples
- **Soft/SDF brush** — gaussian weight `exp(-t²·3)` where `t = dist/radius`;
  blends via standard alpha compositing — produces smooth, painter-style strokes
- **Spray** — uniform-area random scatter (√-distributed radius) so density is
  even across the disc, not clustered at the centre

### Filter pipeline
All filters operate on the CPU pixel buffer in one pass (or two for
convolution kernels). They are designed to be stackable — run `Ctrl+G`
several times for progressive blur, combine emboss + edge detect for
a stylised look, etc.

### Performance notes
- `SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC` keeps the display
  path on the GPU; only `SDL_UpdateTexture` copies the pixel buffer each frame.
- `canvas_clear` uses `memset` for black / white fills (fastest path).
- The title bar shows a live FPS counter updated every second.
- Convolution kernels allocate a single temporary buffer and do one
  clamp-to-edge border pass — no extra copies.

## Self-test images
`make test` writes deterministic BMPs to `test-artifacts/`:
- `scene.bmp`
- `fill_regions.bmp`

---

## TODO / future work
- OpenGL / GLSL shader path for real-time GPU filters (bloom, displacement,
  SDF text, etc.)
- Layer stack with blend modes (multiply, screen, overlay …)
- HSV colour picker widget
- PNG load/save via `stb_image`
- Wacom / pressure-sensitive tablet support via SDL2 touch events
- Brush texture / stamp support (custom shapes loaded from BMP)
- Smear / blend tool (reads neighbour pixels as source colour)
