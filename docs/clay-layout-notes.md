# Clay Layout Notes

Clay is cloned locally for study at `vendor/clay`, which is intentionally gitignored.

Upstream: https://github.com/nicbarker/clay  
Local clone commit when these notes were written: `e6cc36941ab2af5d81107617039d6f527a1c660b`

## Useful Style Lessons

- Keep the public API small and data-oriented.
- Put layout data in simple structs, then let renderers consume the resulting rectangles.
- Avoid hidden allocation in hot UI paths; prefer caller-owned memory, fixed arrays, or explicit arenas.
- Keep one implementation unit responsible for geometry, with tests that do not require SDL.
- Make IDs and sizing explicit so UI can be deterministic and easy to screenshot.

## OpenShop Direction

OpenShop should stay plain C first. Do not add Clay as a hard build dependency until there is a clear need for flexible panel docking or resizable layouts.

The first local step is `src/app_layout.c`: a small tested layout helper for the current Photoshop-like shell. It keeps document hit-testing and toolbar geometry independent from SDL rendering.

