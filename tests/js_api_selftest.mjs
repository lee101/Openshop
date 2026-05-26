import assert from "node:assert/strict";
import { BrushShapes, OpenshopDocument, Tools } from "../js/openshop-api.mjs";

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
doc.duplicateLayer(1, "Paint Copy");
doc.translateActive(4, 3);
doc.grayscaleActive();
doc.adjustActiveBrightness(12);
doc.posterizeActive(4);

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
    "duplicateLayer",
    "translateActive",
    "grayscaleActive",
    "adjustActiveBrightness",
    "posterizeActive",
  ]
);

console.log("openshop js api selftest ok");
