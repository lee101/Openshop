import assert from "node:assert/strict";
import { BlendModes, BrushShapes, GradientTypes, OpenshopDocument, SelectionOps, Tools, VfxBrushes } from "../js/openshop-api.mjs";

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

doc.selectRect(0, 0, 31, 23, SelectionOps.replace);
assert.equal(doc.hasSelection, true);
doc.magicWand(5, 5, 32, SelectionOps.add);
doc.featherSelection(2);
doc.invertSelection();
doc.deselect();
assert.equal(doc.hasSelection, false);
doc.gradientFill({ x0: 0, y0: 0, x1: 63, y1: 0, startColor: 0xff000000, endColor: 0xffffffff, type: GradientTypes.linear });
doc.resizeImage(32, 24);
assert.equal(doc.width, 32);
doc.resizeCanvas(48, 36, 8, 6);
assert.equal(doc.width, 48);
doc.crop(8, 6, 39, 29);
assert.equal(doc.width, 32);
assert.equal(doc.height, 24);
doc.savePsd("out.psd");
doc.selectPolygon([[2, 2], [20, 2], [2, 20]]);
assert.equal(doc.hasSelection, true);
doc.deselect();
doc.addLayerMask(1);
assert.equal(doc.layers[1].hasMask, true);
doc.setLayerMaskEnabled(1, false);
assert.equal(doc.layers[1].maskEnabled, false);
doc.removeLayerMask(1);
assert.equal(doc.layers[1].hasMask, false);
doc.setLayerClipping(1, true);
assert.equal(doc.layers[1].clipping, true);
doc.setLayerClipping(1, false);
doc.cloneStroke({ offsetX: -8, offsetY: 0, x0: 10, y0: 10, x1: 20, y1: 10, radius: 4 });
doc.dodgeBurnStroke({ x0: 4, y0: 4, x1: 12, y1: 4, radius: 3, burn: true });
doc.spongeStroke({ x0: 4, y0: 8, x1: 12, y1: 8, radius: 3 });
doc.smudgeStroke({ x0: 4, y0: 12, x1: 12, y1: 12, radius: 3 });
doc.drawText({ x: 2, y: 2, text: "Hi", scale: 2, color: 0xff000000 });
assert.throws(() => doc.selectPolygon([[0, 0], [1, 1]]));
assert.throws(() => doc.setLayerMaskEnabled(1, true));
assert.throws(() => doc.setLayerClipping(0, true));
assert.throws(() => doc.drawText({ x: 0, y: 0, text: "", scale: 1, color: 0 }));
assert.throws(() => doc.selectRect(0, 0, 5, 5, "bogus"));
assert.throws(() => doc.gradientFill({ x0: 0, y0: 0, x1: 1, y1: 1, startColor: 0, endColor: 0, type: "bogus" }));
assert.throws(() => doc.crop(100, 100, 200, 200));

assert.equal(doc.layers[1].blendMode, BlendModes.multiply);
assert.equal(doc.layers[2].blendMode, BlendModes.multiply);
assert.throws(() => doc.setLayerBlendMode(1, "bogus"));
assert.throws(() => doc.drawVfxStroke({ x0: 0, y0: 0, x1: 1, y1: 1, radius: 2, color: 0, preset: "bogus" }));

assert.equal(doc.width, 32);
assert.equal(doc.height, 24);
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
    "selectRect",
    "magicWand",
    "featherSelection",
    "invertSelection",
    "deselect",
    "gradientFill",
    "resizeImage",
    "resizeCanvas",
    "crop",
    "savePsd",
    "selectPolygon",
    "deselect",
    "addLayerMask",
    "setLayerMaskEnabled",
    "removeLayerMask",
    "setLayerClipping",
    "setLayerClipping",
    "cloneStroke",
    "dodgeBurnStroke",
    "spongeStroke",
    "smudgeStroke",
    "drawText",
  ]
);

console.log("openshop js api selftest ok");
