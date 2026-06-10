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

## OpenShop JS API Roadmap

Shipped in `js/photoshop-api.mjs`:

- `app.documents.add`, `app.activeDocument`, `document.artLayers`, `document.activeLayer`.
- `artLayers.add`, `layer.remove`, `layer.name`, `layer.visible`, `layer.opacity`, `layer.allLocked`, `layer.blendMode`.
- Layer methods: `invert`, `desaturate`, `posterize`, `threshold`, `adjustBrightnessContrast`, `adjustLevels`, `adjustHueSaturation`, `applyGaussianBlur`, `applySharpen`, `translate`, `duplicate`, `merge`.
- `document.flatten`, `document.mergeVisibleLayers`.
- `app.batchPlay(descriptors)` for `make`, `set`, `delete`, `flattenImage`, `invert`, `desaturate`, `gaussianBlur`, `brightnessEvent`, `hueSaturation` descriptors.

Also shipped: `document.selection` (selectAll/deselect/invert/selectRectangle/selectEllipse/magicWand/feather with `SelectionType`), `document.resizeImage`, `document.resizeCanvas` with `AnchorPosition`, `document.crop`, `document.gradientFill` with `GradientType`, `document.saveAs("*.psd")`, and batchPlay `imageSize`/`canvasSize`/`crop` descriptors.

Round 3 additions: `selection.select(points)` polygon lasso, `layer.grouped` (clipping masks), `layer.addLayerMask`/`removeLayerMask`/`setLayerMaskEnabled`/`hasLayerMask`, retouch strokes (`cloneStroke`, `dodge`, `burn`, `sponge`, `smudge`), and `layer.drawText`.

Next:

- Editable text layer objects and path selections in the DOM facade.
- Script execution entry point for `.mjs` files that can call the same command facade as tests.
- Wire the JS command log to the C ABI through an embedded runtime or IPC bridge so recorded commands replay against real documents.

