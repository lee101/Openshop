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
- `Left Mouse`: draw
- `Right Mouse` / `I`: eyedropper (pick color)
- `B`: brush
- `E`: eraser
- `L`: line tool
- `R`: rectangle tool
- `O`: ellipse tool
- `Shift` (with line/rect/ellipse): constrain angle or square/circle
- `[ / ]`: brush size down/up
- `1-6`: quick colors (black, red, green, blue, yellow, purple)
- `F`: flood fill at cursor
- `C`: clear canvas
- `Ctrl+S`: save to `output.bmp`
- `Ctrl+O`: load from `input.bmp`
- `Ctrl+Z` / `Ctrl+Y`: undo / redo
- `Esc`: quit

## Notes
- Canvas size is 800x600; window is 1024x768.
- Load/save uses BMP via SDL built-ins for now.
