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

When `make` is unavailable, the non-SDL tests can also be built and run directly:

```bash
bash tools/run_tests.sh
```

## Script APIs
OpenShop now exposes the same first-pass document/layer/tool command names in C and JavaScript:

- C: `src/openshop_api.h`
- JS: `js/openshop-api.mjs`

Both APIs cover document creation, layer operations, brush/eraser strokes, basic shapes, transforms, flattening, and export plumbing. `make test` runs selftests for both surfaces when Node is available.

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
External UI references are cloned into gitignored `vendor/` directories. Clay is kept at `vendor/clay` for studying its single-header, renderer-agnostic C layout style; see `docs/clay-layout-notes.md`.

## Run
```bash
./openshop [optional_input.bmp]
```

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
- `Esc`: quit

## Notes
- Canvas size is 800x600; window is 1024x768.
- Layer stack starts with a white background layer; new layers are transparent.
- Maximum of 8 layers are supported; direct layer shortcuts target slots 1-8.
- Failed edits, unchanged edits, failed internal snapshot capture/compare steps, and failed undo/redo step applications preserve existing undo/redo stacks and retained snapshots.
- Undo/redo retains up to 20 layer-aware history states; new edits clear redo, while unchanged edits skip dead history entries.
- Locked layers stay visible in the stack but reject paint, fill, clear, transform, load, merge, flatten, stamp, and delete operations.
- Solo preview still renders the active layer even if that layer's normal visibility is off.
- Transparent regions render over a checkerboard preview in the editor.
- Load/save uses BMP via SDL built-ins for now.

## Self-test Images
`make test` now generates deterministic BMP outputs at `test-artifacts/`:
- `scene.bmp`
- `fill_regions.bmp`
