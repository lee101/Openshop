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
- `Shift` + `PageUp / PageDown`: cycle between visible layers only
- `Ctrl` + `PageUp / PageDown`: cycle between hidden layers only
- `Ctrl` + `Shift` + `PageUp / PageDown`: reveal and focus the next hidden layer
- `Ctrl` + `Home / End`: jump to the bottommost or topmost visible layer
- `Ctrl` + `Shift` + `Home / End`: jump to the bottommost or topmost hidden layer
- `Alt` + `PageUp / PageDown`: cycle between locked layers only
- `Alt` + `Home / End`: jump to the bottommost or topmost locked layer
- `Ctrl` + `Alt` + `PageUp / PageDown`: cycle between unlocked layers only
- `Ctrl` + `Alt` + `Home / End`: jump to the bottommost or topmost unlocked layer
- `Alt` + `Shift` + `PageUp / PageDown`: cycle between visible unlocked layers only
- `Alt` + `Shift` + `Home / End`: jump to the bottommost or topmost visible unlocked layer
- `Ctrl+,`: insert a new transparent layer below the active layer
- `Ctrl+N`: insert a new transparent layer above the active layer
- `Ctrl+Shift+N`: add a new transparent layer
- `Ctrl+1` ... `Ctrl+8`: select layer 1-8 directly
- `Ctrl+D`: duplicate active layer
- `Delete` / `Backspace`: delete active layer
- `Ctrl+]` / `Ctrl+[`: move active layer up/down
- `Ctrl+- / Ctrl+=`: active layer opacity down/up
- `Ctrl+0`: reset active layer opacity to 100%
- `Ctrl+A`: show all layers and clear solo mode
- `Ctrl+Shift+I`: invert layer visibility and keep at least the active layer visible
- `Ctrl+Alt+I`: show only the layers that were hidden
- `Ctrl+Shift+R`: reveal the active layer without changing other visibility states
- `Ctrl+Shift+/`: isolate the active layer by hiding all other layers
- `Ctrl+Shift+H`: hide the active layer and jump to the next visible layer
- `Ctrl+Shift+J`: hide the active layer and jump to the previous visible layer
- `Ctrl+Shift+L`: toggle active layer lock
- `Alt+L`: lock the active layer and jump to the next unlocked layer
- `Alt+U`: unlock all layers
- `Ctrl+Alt+L`: show only locked layers
- `Ctrl+Alt+U`: show only unlocked layers
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
- Locked layers stay visible in the stack but reject paint, fill, clear, transform, load, merge, flatten, stamp, and delete operations.
- Solo preview still renders the active layer even if that layer's normal visibility is off.
- Transparent regions render over a checkerboard preview in the editor.
- Load/save uses BMP via SDL built-ins for now.

## Self-test Images
`make test` now generates deterministic BMP outputs at `test-artifacts/`:
- `scene.bmp`
- `fill_regions.bmp`
