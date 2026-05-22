# Photoshop Shortcut Parity

OpenShop should prefer Photoshop-familiar shortcuts when the command exists. Keep old OpenShop shortcuts as aliases until a shortcut editor exists.

| Photoshop action | Common Photoshop shortcut | OpenShop status |
| --- | --- | --- |
| Brush tool | `B` | `B` selects brush. |
| Eraser tool | `E` | `E` selects eraser. |
| Shape tools | `U` | `U` aliases rectangle shape; `R` remains rectangle. |
| Rectangular marquee | `M` | `M` aliases rectangle tool until real selection tools exist. |
| Gradient/Paint bucket group | `G` | `G` aliases fill; `F` remains fill. |
| Eyedropper | `I` | `I` samples active-layer color. |
| Brush size down/up | `[` / `]` | Supported. |
| Undo / redo | `Ctrl+Z`, `Ctrl+Y` | Supported by history shortcuts. |
| New layer | `Shift+Ctrl+N` in Photoshop | OpenShop layer shortcuts are implemented separately; align once modifier routing is expanded. |
| Move tool | `V` | Not mapped yet because `V` currently flips vertically and no dedicated move tool exists. |
| Zoom tool | `Z` | Not mapped yet because zoom/navigation model is not implemented. |
| Swap foreground/background | `X` | Not mapped yet because `X` currently inverts pixels and swatches are not implemented. |
| Default colors | `D` | Needs foreground/background swatch model first. |

Official Adobe shortcut reference: https://helpx.adobe.com/ca/photoshop/desktop/get-started/settings-and-preferences/view-keyboard-shortcuts.html

