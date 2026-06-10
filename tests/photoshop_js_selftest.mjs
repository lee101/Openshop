import assert from "node:assert/strict";
import { Application, BlendMode, VfxBrushes } from "../js/photoshop-api.mjs";

const app = new Application();
assert.equal(app.name, "OpenShop");
assert.equal(app.documents.length, 0);
assert.throws(() => app.activeDocument);

const doc = app.documents.add(640, 480, 72, "Hero Art");
assert.equal(app.documents.length, 1);
assert.equal(app.activeDocument, doc);
assert.equal(doc.width, 640);
assert.equal(doc.height, 480);
assert.equal(doc.name, "Hero Art");
assert.equal(doc.artLayers.length, 1);
assert.equal(doc.backgroundLayer.name, "Background");

const paint = doc.artLayers.add();
assert.equal(doc.artLayers.length, 2);
assert.equal(doc.activeLayer.name, paint.name);

paint.name = "Paint";
paint.opacity = 80;
paint.blendMode = BlendMode.MULTIPLY;
assert.equal(paint.name, "Paint");
assert.equal(paint.opacity, 80);
assert.equal(paint.blendMode, BlendMode.MULTIPLY);
assert.throws(() => {
  paint.blendMode = "notARealMode";
});

paint.invert();
paint.desaturate();
paint.adjustBrightnessContrast(20, 10);
paint.adjustLevels(10, 240, 1.2, 0, 255);
paint.applyGaussianBlur(2);
paint.applySharpen(120);
paint.adjustHueSaturation(45, 30, 0);
paint.posterize(4);
paint.threshold(128);
paint.translate(4, 3);

const dupe = paint.duplicate();
assert.equal(doc.artLayers.length, 3);
assert.equal(dupe.name, "Paint copy");
assert.equal(dupe.blendMode, BlendMode.MULTIPLY);

const byName = doc.artLayers.getByName("Paint");
assert.equal(byName._index, paint._index);
assert.throws(() => doc.artLayers.getByName("Missing"));

const names = [...doc.artLayers].map((layer) => layer.name);
assert.deepEqual(names, ["Background", "Paint", "Paint copy"]);

dupe.merge();
assert.equal(doc.artLayers.length, 2);

doc._backing.drawVfxStroke({ x0: 10, y0: 10, x1: 100, y1: 10, radius: 6, color: 0xff00ffff, preset: VfxBrushes.glow, seed: 7 });

doc.flatten();
assert.equal(doc.artLayers.length, 1);
assert.equal(doc.activeLayer.blendMode, BlendMode.NORMAL);

const results = app.batchPlay([
  { _obj: "make", _target: [{ _ref: "layer" }] },
  { _obj: "set", _target: [{ _ref: "layer" }], to: { opacity: 50, blendMode: BlendMode.SCREEN, name: "FX" } },
  { _obj: "gaussianBlur", radius: 3 },
  { _obj: "hueSaturation", hue: 15, saturation: 10 },
  { _obj: "flattenImage" },
]);
assert.equal(results.length, 5);
assert.equal(doc.artLayers.length, 1);
assert.throws(() => app.batchPlay([{ _obj: "unknownDescriptor" }]));

const commandTypes = doc.commands().map((command) => command.type);
assert.ok(commandTypes.includes("setLayerBlendMode"));
assert.ok(commandTypes.includes("drawVfxStroke"));
assert.ok(commandTypes.includes("adjustBrightnessContrast"));
assert.ok(commandTypes.includes("flatten"));

const second = app.documents.add(64, 64);
assert.equal(app.documents.length, 2);
assert.equal(app.activeDocument, second);
assert.equal(app.documents.getByName("Hero Art"), doc);

console.log("photoshop js api selftest ok");
