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

## SDL I/O Smoke Test
```bash
make test-sdl
```

## Run
```bash
./openshop [optional_input.bmp|optional_input.png]
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
- `1-9`: quick colors (black, red, green, blue, yellow, purple, orange, cyan, white)
- `F`: flood fill on the active layer
- `C`: clear the active layer
- `H`: flip the active layer horizontally
- `V`: flip the active layer vertically
- `J`: rotate the active layer 180 degrees
- `X`: invert the active layer colors (RGB)
- `K`: rotate the active layer 90 degrees clockwise (content centered, edges cropped/padded)
- `Shift+K`: rotate the active layer 90 degrees counter-clockwise
- `G`: desaturate (grayscale) the active layer
- `Q`: posterize the active layer (4 levels per channel)
- `N`: threshold the active layer to black/white (BT.709 luminance, midpoint 128)
- `W`: apply sepia tone to the active layer
- `Shift+H`: rotate hue of active layer by 30° (RGB↔HSV; preserves S and V)
- `Shift+[` / `Shift+]`: decrease/increase active layer contrast (scales RGB around mid-gray)
- `Arrow Keys`: nudge the active layer by 1 pixel
- `Shift` + `Arrow Keys`: nudge the active layer by 10 pixels
- `PageUp / PageDown`: cycle active layer
- `Ctrl+,`: insert a new transparent layer below the active layer
- `Ctrl+N`: insert a new transparent layer above the active layer
- `Ctrl+Shift+N`: add a new transparent layer
- `Ctrl+1` ... `Ctrl+9`: select layer 1-9 directly
- `Ctrl+D`: duplicate active layer
- `Delete` / `Backspace`: delete active layer
- `Ctrl+]` / `Ctrl+[`: move active layer up/down
- `Ctrl+- / Ctrl+=`: active layer opacity down/up
- `Ctrl+0`: reset active layer opacity to 100%
- `Ctrl+B`: apply auto-levels to the active layer (per-channel histogram stretch)
- `Ctrl+P`: increase active layer brightness by 15 (clamps to [0,255] per channel)
- `Ctrl+Shift+P`: decrease active layer brightness by 15
- `Ctrl+A`: show all layers and clear solo mode
- `Ctrl+Shift+R`: reveal the active layer without changing other visibility states
- `Ctrl+R`: rename the active layer
- `Ctrl+Shift+H`: hide the active layer and jump to the next visible layer
- `Ctrl+Shift+L`: toggle active layer lock
- `Ctrl+/`: solo active layer on/off
- `Ctrl+Shift+V`: toggle active layer visibility
- `Ctrl+M`: merge active layer down
- `Ctrl+U`: merge active layer up
- `Ctrl+Shift+E`: stamp visible image into active layer
- `Ctrl+Shift+G`: stamp visible image into a new top layer
- `Ctrl+Shift+M`: flatten visible layers
- `Ctrl+S`: save to the current session BMP path, or reuse the session PNG path if that is the only existing default output
- `Ctrl+Shift+S`: save the composited image to `output.png`
- `Ctrl+O`: load `input.bmp`, or fall back to `input.png` if the BMP is missing
- `Ctrl+Shift+O`: load `input.png` into the active layer
- `Ctrl+F`: tolerance-based fill from the cursor (exact `F` fill is unchanged)
- `Ctrl+Shift+B`: apply unsharp-mask sharpening to the active layer
- `Shift+S`: apply edge sharpening to the active layer
- `Ctrl+Z` / `Ctrl+Y`: undo / redo (layer-aware)
- PNG import/export uses bundled `stb_image` / `stb_image_write` headers and does not depend on SDL image codecs.
- `Esc`: quit

## Notes
- Canvas size is 800x600; window is 1024x768.
- Layer stack starts with a white background layer; new layers are transparent.
- Maximum of 16 layers supported. `Ctrl+1`...`Ctrl+9` selects layers 1–9 directly.
- Locked layers stay visible in the stack but reject paint, fill, clear, transform, load, merge, flatten, stamp, and delete operations.
- Solo preview still renders the active layer even if that layer's normal visibility is off.
- Transparent regions render over a checkerboard preview in the editor.
- Format routing:
  Startup input uses the path you pass on the CLI and auto-detects BMP vs PNG by extension, with a BMP fallback for unknown extensions.
  Launching with a startup file also seeds the default `Ctrl+O` pair from that file stem, so `./openshop art/scene.png` reloads `art/scene.bmp` or `art/scene.png`.
  `Ctrl+O` prefers the session BMP path, falls back to the session PNG path, and `Ctrl+Shift+O` always targets the session PNG path.
  A successful startup load or `Ctrl+O` also seeds the default `Ctrl+S` pair from that loaded file stem; otherwise the session save pair stays `output.bmp` / `output.png`.
  `Ctrl+S` prefers the session BMP path unless only the session PNG path already exists, and `Ctrl+Shift+S` always targets the session PNG path.
  Default load/save status messages note when the PNG alternate path was used.
- `make test-sdl` exercises SDL-backed image I/O routing when SDL2 development tools are installed.

## Self-test Images
`make test` now generates deterministic BMP outputs at `test-artifacts/`:
- `scene.bmp`
- `fill_regions.bmp`
