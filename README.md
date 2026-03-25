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
- `Shift` + `PageUp / PageDown`: cycle active visible layer only
- `Home / End`: jump to the top/bottom layer
- `Shift` + `Home / End`: jump to the top/bottom visible layer
- `Ctrl+,`: insert a new transparent layer below the active layer
- `Ctrl+N`: insert a new transparent layer above the active layer
- `Ctrl+Shift+N`: add a new transparent layer
- `Ctrl+1` ... `Ctrl+8`: select layer 1-8 directly
- `Ctrl+F1` ... `Ctrl+F8`: select layer 9-16 directly
- `Ctrl+Shift+1` ... `Ctrl+Shift+8`: move the active layer to slot 1-8
- `Ctrl+Shift+F1` ... `Ctrl+Shift+F8`: move the active layer to slot 9-16
- `Alt+1` ... `Alt+8`: select the 1st-8th visible layer
- `Alt+F1` ... `Alt+F8`: select the 9th-16th visible layer
- `Alt+Shift+1` ... `Alt+Shift+8`: move the active layer to the 1st-8th visible slot
- `Alt+Shift+F1` ... `Alt+Shift+F8`: move the active layer to the 9th-16th visible slot
- `Ctrl+D`: duplicate active layer
- `Delete` / `Backspace`: delete active layer
- `Ctrl+]` / `Ctrl+[`: move active layer up/down
- `Ctrl+Home / Ctrl+End`: move active layer to the top/bottom
- `Ctrl+Shift+Home / Ctrl+Shift+End`: move the active visible layer to the top/bottom visible slot
- `Ctrl+- / Ctrl+=`: active layer opacity down/up by 10%
- `Ctrl+Shift+- / Ctrl+Shift+=`: active layer opacity down/up by 1%
- `Ctrl+Shift+0`: set active layer opacity to 0%
- `Ctrl+9`: set active layer opacity to 50%
- `Ctrl+0`: reset active layer opacity to 100%
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
- `Ctrl+Z` / `Ctrl+Y`: undo / redo (layer-aware, 20 history states)
- `Esc`: quit

## Notes
- Canvas size is 800x600; window is 1024x768.
- Layer stack starts with a white background layer; new layers are transparent.
- Maximum of 16 layers are supported; `1`-`8` target slots or visible ranks 1-8, while `F1`-`F8` target 9-16.
- The window title shows both the absolute active layer slot and its visible rank; hidden active layers display as `hidden N/M visible`.
- Failed edits, unchanged edits, failed internal snapshot capture/compare steps, and failed undo/redo step applications all preserve the existing undo/redo stacks and their retained snapshots, restoring any destination entry evicted during the attempted step and rolling back discarded temporary step snapshots to an empty state.
- Undo/redo retains up to 20 layer-aware history states; once full, the oldest retained history state is evicted as new ones are recorded, and new edits clear the redo stack.
- Repeating no-op history records are ignored without consuming redo or evicting the oldest retained state, and failed or unchanged edits preserve redo instead of consuming history.
- Visibility shortcuts such as `Ctrl+A` and `Ctrl+Shift+R` only create undo states when they actually change visibility or solo state.
- Brush, eraser, shape, fill, clear, transform, opacity, merge, stamp, and layer-management actions skip dead undo entries when the resulting layer stack is unchanged.
- Direct layer-slot shortcuts print a status message when the requested absolute or visible slot does not exist.
- Visible-only shortcuts print a status message when the active layer is hidden or there is no other visible layer/slot to target.
- Locked layers stay visible in the stack but reject paint, fill, clear, transform, load, merge, flatten, stamp, and delete operations.
- Solo preview still renders the active layer even if that layer's normal visibility is off.
- Transparent regions render over a checkerboard preview in the editor.
- Load/save uses BMP via SDL built-ins for now.

## Self-test Images
`make test` now generates deterministic BMP outputs at `test-artifacts/`:
- `scene.bmp`
- `fill_regions.bmp`
