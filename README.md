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
./openshop [optional_input.bmp]
```

## Controls
- `Left Mouse`: draw
- `Right Mouse` / `I`: eyedropper (pick color)
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
- `- / =`: opacity down/up (1%-100%)
- `1-6`: quick colors (black, red, green, blue, yellow, purple)
- `F`: flood fill at cursor
- `C`: clear canvas
- `H`: flip canvas horizontally
- `V`: flip canvas vertically
- `J`: rotate canvas 180 degrees
- `X`: invert canvas colors (RGB)
- `Ctrl+S`: save to `output.bmp`
- `Ctrl+O`: load from `input.bmp`
- `Ctrl+Z` / `Ctrl+Y`: undo / redo
- `Esc`: quit

## Notes
- Canvas size is 800x600; window is 1024x768.
- Load/save uses BMP via SDL built-ins for now.

## Self-test Images
`make test` now generates deterministic BMP outputs at `test-artifacts/`:
- `scene.bmp`
- `fill_regions.bmp`
