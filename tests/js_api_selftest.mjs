import assert from "node:assert/strict";
import { BrushShapes, MAX_LAYERS, OpenshopDocument, Tools } from "../js/openshop-api.mjs";

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

assert.equal(doc.width, 64);
assert.equal(doc.height, 48);
assert.equal(doc.layers.length, 3);
assert.equal(doc.activeLayer, 2);
assert.equal(doc.dirty, true);
assert.deepEqual(
  doc.commands.map((command) => command.type),
  ["addLayer", "drawStroke", "drawShape", "setLayerOpacity", "duplicateLayer", "translateActive"]
);

assert.equal(MAX_LAYERS, 8);
assert.throws(() => doc.selectLayer(99), RangeError);
assert.throws(() => doc.setLayerOpacity(-1, 50), RangeError);

const capped = new OpenshopDocument({ width: 8, height: 8 });
for (let i = capped.layers.length; i < MAX_LAYERS; i++) {
  capped.addLayer(`Layer ${i}`);
}
assert.equal(capped.layers.length, MAX_LAYERS);
assert.throws(() => capped.addLayer("Overflow"), RangeError);
assert.throws(() => capped.insertLayer(0, "Overflow"), RangeError);
assert.throws(() => capped.duplicateLayer(0), RangeError);

const singleLayer = new OpenshopDocument({ width: 4, height: 4 });
assert.throws(() => singleLayer.deleteLayer(0), RangeError);

console.log("openshop js api selftest ok");
