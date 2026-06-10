import assert from "node:assert/strict";
import { AnchorPosition, Application, BlendMode, SelectionType, VfxBrushes } from "../js/photoshop-api.mjs";

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

assert.equal(doc.selection.exists, false);
doc.selection.selectAll();
assert.equal(doc.selection.exists, true);
doc.selection.selectRectangle({ left: 10, top: 10, right: 100, bottom: 100 });
doc.selection.selectEllipse({ left: 20, top: 20, right: 80, bottom: 80 }, SelectionType.EXTEND);
doc.selection.magicWand(50, 50, 24, SelectionType.DIMINISH);
doc.selection.feather(3);
doc.selection.invert();
doc.selection.deselect();
assert.equal(doc.selection.exists, false);

doc.resizeImage(320, 240);
assert.equal(doc.width, 320);
doc.resizeCanvas(400, 300, AnchorPosition.MIDDLECENTER);
assert.equal(doc.width, 400);
assert.throws(() => doc.resizeCanvas(10, 10, "bogusAnchor"));
doc.crop({ left: 40, top: 30, right: 359, bottom: 269 });
assert.equal(doc.width, 320);
assert.equal(doc.height, 240);
doc.gradientFill({ x0: 0, y0: 0, x1: 320, y1: 0, startColor: 0xff112233, endColor: 0xffeeddcc });
doc.saveAs("art.psd");
assert.throws(() => doc.saveAs("art.tiff"));

app.batchPlay([
  { _obj: "imageSize", width: 160, height: 120 },
  { _obj: "canvasSize", width: 200, height: 150, anchor: AnchorPosition.TOPLEFT },
  { _obj: "crop", to: { left: 0, top: 0, right: 159, bottom: 119 } },
]);
assert.equal(doc.width, 160);
assert.equal(doc.height, 120);

const selectionCommands = doc.commands().map((command) => command.type);
assert.ok(selectionCommands.includes("selectAll"));
assert.ok(selectionCommands.includes("magicWand"));
assert.ok(selectionCommands.includes("resizeImage"));
assert.ok(selectionCommands.includes("crop"));
assert.ok(selectionCommands.includes("savePsd"));

const fx = doc.artLayers.add();
fx.addLayerMask();
assert.equal(fx.hasLayerMask, true);
fx.setLayerMaskEnabled(false);
fx.removeLayerMask(true);
assert.equal(fx.hasLayerMask, false);
fx.grouped = true;
assert.equal(fx.grouped, true);
fx.grouped = false;
doc.selection.select([[10, 10], [60, 10], [10, 60]], SelectionType.REPLACE);
assert.equal(doc.selection.exists, true);
doc.selection.deselect();
fx.cloneStroke({ offsetX: -10, offsetY: 0, x0: 30, y0: 30, x1: 60, y1: 30, radius: 5 });
fx.dodge({ x0: 10, y0: 10, x1: 40, y1: 10, radius: 4 });
fx.burn({ x0: 10, y0: 20, x1: 40, y1: 20, radius: 4 });
fx.sponge({ x0: 10, y0: 30, x1: 40, y1: 30, radius: 4 });
fx.smudge({ x0: 10, y0: 40, x1: 40, y1: 40, radius: 4 });
fx.drawText({ x: 8, y: 8, text: "OpenShop", scale: 2, color: 0xffffffff });
{
  const types = doc.commands().map((command) => command.type);
  assert.ok(types.includes("addLayerMask"));
  assert.ok(types.includes("selectPolygon"));
  assert.ok(types.includes("cloneStroke"));
  assert.ok(types.includes("drawText"));
}

const second = app.documents.add(64, 64);
assert.equal(app.documents.length, 2);
assert.equal(app.activeDocument, second);
assert.equal(app.documents.getByName("Hero Art"), doc);

console.log("photoshop js api selftest ok");
