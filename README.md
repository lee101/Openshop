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

## Run
```bash
./openshop [optional_input.bmp]
```

## Controls
- `Left Mouse`: draw (uses current brush shape)
- `Right Mouse` / `I`: eyedropper (pick the visible color at cursor)
- `B`: brush
- `E`: eraser (removes alpha on the active layer)
- `L`: line tool
- `R`: rectangle tool
- `O`: ellipse tool
- `Shift` (with line/rect/ellipse): constrain angle or square/circle
- `[ / ]`: brush size down/up
- `- / =`: opacity down/up (1%-100%)
- `, / .`: cycle brush shape (round, square, diamond)
- `1-6`: quick colors (black, red, green, blue, yellow, purple)
- `F`: flood fill at cursor (active layer)
- `C`: clear the active layer (background layer resets to white, others become transparent)
- `PageUp / PageDown`: cycle through layers
- `Ctrl+Shift+N`: add a new transparent layer on top
- `Ctrl+Shift+V`: toggle visibility of the active layer
- `Ctrl+S`: save the composited image to `output.bmp`
- `Ctrl+O`: load `input.bmp` into the current layer
- `Ctrl+Z` / `Ctrl+Y`: undo / redo (layer-aware)
- `Esc`: quit

## Notes
- Canvas size is 800x600; window is 1024x768.
- Layer stack starts with a white background layer; additional layers are transparent and alpha-blend in display/save output.
- Load/save uses BMP via SDL built-ins for now.

## Self-test Images
`make test` now generates deterministic BMP outputs at `test-artifacts/`:
- `scene.bmp`
- `fill_regions.bmp`
