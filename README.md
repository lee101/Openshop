# Openshop
Photoshop alternative (minimal C prototype)

## Build
Requires SDL2 development packages.

```bash
make
```

## Test (no SDL2 required)
```bash
make test
```

CI builds and tests on Linux, macOS, and Windows (MSYS2/MinGW), including a headless app smoke run under SDL's dummy video driver with the software renderer.

## Script APIs
OpenShop now exposes the same first-pass document/layer/tool command names in C and JavaScript:

- C: `src/openshop_api.h`
- JS: `js/openshop-api.mjs`
- Photoshop-style JS DOM: `js/photoshop-api.mjs` (`app`, `app.documents.add`, `document.artLayers`, `layer.blendMode`, `app.batchPlay`)

Both APIs cover document creation, layer operations, brush/eraser strokes, VFX brush strokes, basic shapes, transforms, layer blend modes, image adjustments (brightness/contrast, hue/saturation, levels, desaturate, posterize, threshold, gaussian blur, sharpen), flattening, and export plumbing. `make test` runs selftests for all surfaces when Node is available.

## Selections
Rectangular marquee (`M`, drag), magic wand (`W`, click, tolerance 32), polygon lasso (script APIs), `Shift` adds, `Alt` subtracts, tiny drag or `Esc` deselects. Selections clip painting, fills, shapes, VFX strokes, retouch strokes, text, gradients, and adjustments (feather supported through the script APIs). Marching ants render over the canvas.

## Layer Masks & Clipping Masks
Per-layer grayscale masks (add from the active selection or reveal-all, enable/disable, discard or apply on removal) and Photoshop-style clipping masks that clip a layer to the alpha of the layer below. Both honored by compositing and merges; scriptable via C and the JS DOM (`layer.addLayerMask()`, `layer.grouped`).

## Retouch Tools & Text
Clone stamp, dodge, burn, sponge (saturate/desaturate), and smudge strokes (`src/retouch.h`), plus bitmap-font text rendering with scaling and newlines (`src/font.h`, `canvas_draw_text`).

## Layer Blend Modes
Normal, Multiply, Screen, Overlay, Soft Light, Hard Light, Darken, Lighten, Color Dodge, Color Burn, Linear Dodge, Linear Burn, Difference, Exclusion. Compositing and merge-down honor the active mode; `Shift+=` / `Shift+-` cycle the active layer's mode.

## VFX Brushes
Deterministic seeded brush engine (`src/brush_engine.h`) with soft falloff, flow, spacing, scatter, size jitter, and additive glow: Soft Round, Airbrush, Splatter, Glow, Sparkle, Smoke.

## Gradients, Crop, Resize, PSD
Linear/radial gradient fills (`src/gradient.h`), image resize (bilinear), canvas resize with offset anchoring, crop (`layer_stack_crop`), and flattened PSD export plus raw/RLE PSD import (`src/psd.h`) — all scriptable from C and JS.

## SDL I/O Smoke Test
```bash
make test-sdl
```

## Visual Bench
Generate deterministic screenshots into the gitignored `visualbench/` directory:

```bash
make visualbench
```

This writes representative BMP captures for the editor overview, layer states, and tool gallery so visual changes can be inspected locally without committing generated images.

Current visualbench also writes per-tool captures for brush, eraser, line, rectangle, ellipse, and fill.

## Local Reference Libraries
Clay is vendored at `third_party/clay` (single-header, zlib license) and drives the workspace layout; see `docs/clay-layout-notes.md`. Other UI references are cloned into gitignored `vendor/` directories.

## Run
```bash
./openshop [WxH] [optional_input.bmp]
```

Documents can be any size (e.g. `./openshop 1920x1080`); the window is resizable and the workspace relayouts responsively. The document view auto-fits, with wheel zoom and middle-drag pan.

## SDF UI + Clay Layout
The workspace chrome is laid out with Clay (`third_party/clay`, zlib license) and painted by an in-house SDF renderer (`src/ui_sdf.h`): rounded rects, borders, capsule lines, and text are evaluated as signed distance fields with analytic anti-aliasing into a draw list, then presented through SDL's GPU renderer (Direct3D on Windows, Metal on macOS, OpenGL/Vulkan on Linux — NVIDIA, AMD, Intel, and Apple GPUs all use the same path, with automatic software fallback when no GPU driver is available). The draw-list model keeps the door open for a native shader backend. The shell (menus, options bar, tool rail, layers panel, swatches, status bar) is rebuilt only when state changes, hashed per frame.

## Controls
- `Left Mouse`: draw on the active layer
- `Right Mouse` / `I`: eyedropper (pick visible color)
- `Right Mouse` while shaping / `Esc`: cancel shape preview
- `B`: brush
- `E`: eraser
- `L`: line tool
- `R`: rectangle tool
- `T`: filled rectangle tool
- `O`: ellipse tool
- `P`: filled ellipse tool
- `Shift` (with line/rect/ellipse, filled variants included): constrain angle or square/circle
- `[ / ]`: brush size down/up
- `, / .`: cycle brush shape (round, square, diamond)
- `- / =`: opacity down/up (1%-100%)
- `1-6`: quick colors (black, red, green, blue, yellow, purple)
- `F`: flood fill on the active layer
- `C`: clear the active layer
- `H`: flip the active layer horizontally
- `V`: flip the active layer vertically
- `J`: rotate the active layer 180 degrees
- `X`: invert the active layer colors (RGB)
- `Arrow Keys`: nudge the active layer by 1 pixel
- `Shift` + `Arrow Keys`: nudge the active layer by 10 pixels
- `PageUp / PageDown`: cycle active layer
- `Ctrl+,`: insert a new transparent layer below the active layer
- `Ctrl+N`: insert a new transparent layer above the active layer
- `Ctrl+Shift+N`: add a new transparent layer
- `Ctrl+1` ... `Ctrl+8`: select layer 1-8 directly
- `Ctrl+Shift+1` ... `Ctrl+Shift+8`: solo layer 1-8 directly
- `Ctrl+Alt+1` ... `Ctrl+Alt+8`: toggle visibility for layer 1-8 directly
- `Ctrl+Alt+Shift+1` ... `Ctrl+Alt+Shift+8`: toggle lock for layer 1-8 directly
- `Ctrl+D`: duplicate active layer
- `Delete` / `Backspace`: delete active layer
- `Ctrl+]` / `Ctrl+[`: move active layer up/down
- `Ctrl+Shift+]` / `Ctrl+Shift+[` : send active layer to top/bottom
- `Ctrl+- / Ctrl+=`: active layer opacity down/up
- `Ctrl+0`: reset active layer opacity to 100%
- `F2`: reset the active layer name to the next available default label
- `Shift+F2`: reset non-background locked layer names to default labels
- `Alt+Shift+F2`: reset non-background visible layer names to default labels
- `Alt+F2`: reset locked layer names to default labels
- `Ctrl+Alt+Shift+F2`: reset non-background unlocked layer names to default labels
- `Ctrl+Alt+F2`: reset visible layer names to default labels
- `Ctrl+Shift+F2`: reset unlocked layer names to default labels
- `Ctrl+F2`: reset all layer names to default labels
- `Ctrl+A`: show all layers and clear solo mode
- `Ctrl+Shift+R`: reveal the active layer without changing other visibility states
- `Ctrl+Shift+H`: hide the active layer and jump to the next visible layer
- `Ctrl+Shift+L`: toggle active layer lock
- `Ctrl+/`: solo active layer on/off
- `Ctrl+Shift+V`: toggle active layer visibility
- `Ctrl+M`: merge active layer down
- `Ctrl+U`: merge active layer up
- `Ctrl+Shift+E`: stamp visible image into active layer
- `Ctrl+Shift+G`: stamp visible image into a new top layer
- `Ctrl+Shift+M`: flatten visible layers
- `Ctrl+S`: save the composited image to `output.bmp`
- `Ctrl+O`: load `input.bmp` into the active layer
- `Ctrl+Z` / `Ctrl+Y`: undo / redo (layer-aware)
- `Shift+=` / `Shift+-`: cycle active layer blend mode forward/back
- `Mouse Wheel`: zoom in/out around the cursor
- `Middle Mouse` drag: pan the document
- `0`: zoom to fit
- `Tab`: toggle compact mode (hide panels, Photoshop-style)
- `M`: rectangular marquee mode (drag to select; `Shift` adds, `Alt` subtracts)
- `W`: magic wand mode (click to select; `Shift` adds, `Alt` subtracts)
- `Esc`: deselect if a selection is active, otherwise quit

## Notes
- Document size is arbitrary (default 800x600); the window is resizable (default 1280x800).
- Layer stack starts with a white background layer; new layers are transparent.
- Locked layers stay visible in the stack but reject paint, fill, clear, transform, load, merge, flatten, stamp, and delete operations.
- Solo preview still renders the active layer even if that layer's normal visibility is off.
- Transparent regions render over a checkerboard preview in the editor.
- Load/save uses BMP via SDL built-ins for now.

## Self-test Images
`make test` now generates deterministic BMP outputs at `test-artifacts/`:
- `scene.bmp`
- `fill_regions.bmp`
