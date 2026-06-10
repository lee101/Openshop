import assert from "node:assert/strict";
import { BlendModes, BrushShapes, OpenshopDocument, Tools, VfxBrushes } from "../js/openshop-api.mjs";

const doc = new OpenshopDocument({ width: 64, height: 48 });

doc.addLayer("Paint");
doc.drawStroke({
  x0: 8,
  y0: 8,
  x1: 40,
  y1: 8,
  radius: 4,
  color: 0xffff0000,
  shape: BrushShapes.round,
});
doc.drawShape({
  tool: Tools.filledRect,
  x0: 12,
  y0: 18,
  x1: 30,
  y1: 30,
  color: 0x800000ff,
});
doc.setLayerOpacity(1, 80);
doc.setLayerBlendMode(1, BlendModes.multiply);
doc.duplicateLayer(1, "Paint Copy");
doc.translateActive(4, 3);
doc.drawVfxStroke({ x0: 4, y0: 40, x1: 60, y1: 40, radius: 5, color: 0xff00ffff, preset: VfxBrushes.splatter, seed: 99 });
doc.adjustBrightnessContrast(10, 5);
doc.adjustHueSaturation(30, 15);
doc.adjustLevels({ inBlack: 8, inWhite: 248 });
doc.blurActive(2);
doc.sharpenActive(80);

assert.equal(doc.layers[1].blendMode, BlendModes.multiply);
assert.equal(doc.layers[2].blendMode, BlendModes.multiply);
assert.throws(() => doc.setLayerBlendMode(1, "bogus"));
assert.throws(() => doc.drawVfxStroke({ x0: 0, y0: 0, x1: 1, y1: 1, radius: 2, color: 0, preset: "bogus" }));

assert.equal(doc.width, 64);
assert.equal(doc.height, 48);
assert.equal(doc.layers.length, 3);
assert.equal(doc.activeLayer, 2);
assert.equal(doc.dirty, true);
assert.deepEqual(
  doc.commands.map((command) => command.type),
  [
    "addLayer",
    "drawStroke",
    "drawShape",
    "setLayerOpacity",
    "setLayerBlendMode",
    "duplicateLayer",
    "translateActive",
    "drawVfxStroke",
    "adjustBrightnessContrast",
    "adjustHueSaturation",
    "adjustLevels",
    "blurActive",
    "sharpenActive",
  ]
);

console.log("openshop js api selftest ok");
