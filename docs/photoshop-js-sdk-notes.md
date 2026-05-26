# Photoshop JavaScript API Notes

These notes summarize official Adobe sources for matching a Photoshop-like automation surface. They are reference links, not vendored Adobe docs.

## Official Sources

- Photoshop UXP API reference: https://developer.adobe.com/photoshop/uxp/ps_reference
- `batchPlay` details: https://developer.adobe.com/photoshop/uxp/2022/ps_reference/media/batchplay/
- UXP scripting in Photoshop: https://developer.adobe.com/photoshop/uxp/2022/ps_reference/media/uxpscripting/
- UXP API reference: https://developer.adobe.com/photoshop/uxp/2022/uxp-api/

## Compatibility Direction

- Keep OpenShop's C API as the stable core ABI. JavaScript should be a thin command facade over the same command model.
- Model Photoshop's object shape where it fits: application, documents, layers, active document, active layer, tools, and command execution.
- Prefer high-level DOM-like commands first. Add a lower-level batch command function for operations that do not have a stable high-level wrapper yet.
- Keep batch commands JSON-serializable so they can be sent through local scripts, cloud agents, and future plugin hosts.
- Make mutating commands explicit about history behavior, active layer selection, and composite invalidation.
- Treat fill command colors as straight ARGB layer writes. Semi-transparent and fully transparent fills should preserve the requested 32-bit color instead of blending through the brush pipeline.

## OpenShop JS API Roadmap

- `app.documents`, `app.activeDocument`, `document.layers`, `document.activeLayer`.
- `document.createLayer`, `layer.delete`, `layer.name`, `layer.visible`, `layer.opacity`, `layer.locked`.
- `document.fill`, `document.clear`, `document.resizeCanvas`, `document.transformActiveLayer`.
- `action.batch(commands, options)` for JSON command arrays.
- Script execution entry point for `.mjs` files that can call the same command facade as tests.
