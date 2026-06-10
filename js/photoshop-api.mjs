import { BlendModes, GradientTypes, OpenshopDocument, SelectionOps, VfxBrushes } from "./openshop-api.mjs";

export const BlendMode = Object.freeze({
  NORMAL: BlendModes.normal,
  MULTIPLY: BlendModes.multiply,
  SCREEN: BlendModes.screen,
  OVERLAY: BlendModes.overlay,
  SOFTLIGHT: BlendModes.softLight,
  HARDLIGHT: BlendModes.hardLight,
  DARKEN: BlendModes.darken,
  LIGHTEN: BlendModes.lighten,
  COLORDODGE: BlendModes.colorDodge,
  COLORBURN: BlendModes.colorBurn,
  LINEARDODGE: BlendModes.linearDodge,
  LINEARBURN: BlendModes.linearBurn,
  DIFFERENCE: BlendModes.difference,
  EXCLUSION: BlendModes.exclusion,
});

export const ElementPlacement = Object.freeze({
  PLACEATBEGINNING: "placeAtBeginning",
  PLACEATEND: "placeAtEnd",
});

export const AnchorPosition = Object.freeze({
  TOPLEFT: "topLeft",
  TOPCENTER: "topCenter",
  TOPRIGHT: "topRight",
  MIDDLELEFT: "middleLeft",
  MIDDLECENTER: "middleCenter",
  MIDDLERIGHT: "middleRight",
  BOTTOMLEFT: "bottomLeft",
  BOTTOMCENTER: "bottomCenter",
  BOTTOMRIGHT: "bottomRight",
});

export const SelectionType = Object.freeze({
  REPLACE: SelectionOps.replace,
  EXTEND: SelectionOps.add,
  DIMINISH: SelectionOps.subtract,
});

export const GradientType = Object.freeze({
  LINEAR: GradientTypes.linear,
  RADIAL: GradientTypes.radial,
});

export class DocumentSelection {
  constructor(document) {
    this._document = document;
  }

  get exists() {
    return this._document._backing.hasSelection;
  }

  selectAll() {
    this._document._backing.selectAll();
  }

  deselect() {
    this._document._backing.deselect();
  }

  invert() {
    this._document._backing.invertSelection();
  }

  selectRectangle({ left, top, right, bottom }, type = SelectionType.REPLACE) {
    this._document._backing.selectRect(left, top, right, bottom, type);
  }

  selectEllipse({ left, top, right, bottom }, type = SelectionType.REPLACE) {
    this._document._backing.selectEllipse(left, top, right, bottom, type);
  }

  magicWand(x, y, tolerance = 32, type = SelectionType.REPLACE) {
    this._document._backing.magicWand(x, y, tolerance, type);
  }

  feather(radius) {
    this._document._backing.featherSelection(Math.max(1, Math.round(radius)));
  }

  select(points, type = SelectionType.REPLACE) {
    this._document._backing.selectPolygon(points, type);
  }
}

export { VfxBrushes };

export class ArtLayer {
  constructor(document, index) {
    this._document = document;
    this._index = index;
  }

  get _backing() {
    return this._document._backing.layers[this._index];
  }

  _activate() {
    if (this._document._backing.activeLayer !== this._index) {
      this._document._backing.selectLayer(this._index);
    }
  }

  get name() {
    return this._backing.name;
  }

  set name(value) {
    this._document._backing.renameLayer(this._index, String(value));
  }

  get visible() {
    return this._backing.visible;
  }

  set visible(value) {
    this._document._backing.setLayerVisible(this._index, Boolean(value));
  }

  get opacity() {
    return this._backing.opacityPercent;
  }

  set opacity(value) {
    this._document._backing.setLayerOpacity(this._index, Math.max(0, Math.min(100, Math.round(value))));
  }

  get allLocked() {
    return this._backing.locked;
  }

  set allLocked(value) {
    this._document._backing.setLayerLocked(this._index, Boolean(value));
  }

  get blendMode() {
    return this._backing.blendMode ?? BlendMode.NORMAL;
  }

  set blendMode(value) {
    this._document._backing.setLayerBlendMode(this._index, value);
  }

  invert() {
    this._activate();
    this._document._backing.invertActiveRgb();
  }

  desaturate() {
    this._activate();
    this._document._backing.desaturateActive();
  }

  posterize(levels) {
    this._activate();
    this._document._backing.posterizeActive(levels);
  }

  threshold(level) {
    this._activate();
    this._document._backing.thresholdActive(level);
  }

  adjustBrightnessContrast(brightness, contrast) {
    this._activate();
    this._document._backing.adjustBrightnessContrast(brightness, contrast);
  }

  adjustLevels(inputRangeStart, inputRangeEnd, inputRangeGamma, outputRangeStart, outputRangeEnd) {
    this._activate();
    this._document._backing.adjustLevels({
      inBlack: inputRangeStart,
      inWhite: inputRangeEnd,
      gamma: inputRangeGamma,
      outBlack: outputRangeStart,
      outWhite: outputRangeEnd,
    });
  }

  applyGaussianBlur(radius) {
    this._activate();
    this._document._backing.blurActive(Math.max(1, Math.round(radius)));
  }

  applySharpen(amountPercent = 100) {
    this._activate();
    this._document._backing.sharpenActive(amountPercent);
  }

  adjustHueSaturation(hue, saturation, lightness = 0) {
    this._activate();
    this._document._backing.adjustHueSaturation(hue, saturation, lightness);
  }

  translate(deltaX, deltaY) {
    this._activate();
    this._document._backing.translateActive(Math.round(deltaX), Math.round(deltaY));
  }

  get grouped() {
    return Boolean(this._backing.clipping);
  }

  set grouped(value) {
    this._document._backing.setLayerClipping(this._index, Boolean(value));
  }

  get hasLayerMask() {
    return Boolean(this._backing.hasMask);
  }

  addLayerMask() {
    this._document._backing.addLayerMask(this._index);
  }

  removeLayerMask(apply = false) {
    this._document._backing.removeLayerMask(this._index, apply);
  }

  setLayerMaskEnabled(enabled) {
    this._document._backing.setLayerMaskEnabled(this._index, enabled);
  }

  cloneStroke(options) {
    this._activate();
    this._document._backing.cloneStroke(options);
  }

  dodge(options) {
    this._activate();
    this._document._backing.dodgeBurnStroke({ ...options, burn: false });
  }

  burn(options) {
    this._activate();
    this._document._backing.dodgeBurnStroke({ ...options, burn: true });
  }

  sponge(options) {
    this._activate();
    this._document._backing.spongeStroke(options);
  }

  smudge(options) {
    this._activate();
    this._document._backing.smudgeStroke(options);
  }

  drawText(options) {
    this._activate();
    this._document._backing.drawText(options);
  }

  duplicate() {
    this._document._backing.duplicateLayer(this._index, `${this.name} copy`);
    return new ArtLayer(this._document, this._index + 1);
  }

  merge() {
    this._document._backing.mergeDown(this._index);
    return new ArtLayer(this._document, Math.max(0, this._index - 1));
  }

  remove() {
    this._document._backing.deleteLayer(this._index);
  }
}

export class ArtLayers {
  constructor(document) {
    this._document = document;
  }

  get length() {
    return this._document._backing.layers.length;
  }

  add() {
    this._document._backing.addLayer(`Layer ${this.length}`);
    return new ArtLayer(this._document, this._document._backing.activeLayer);
  }

  item(index) {
    if (index < 0 || index >= this.length) {
      throw new RangeError(`no layer at index ${index}`);
    }
    return new ArtLayer(this._document, index);
  }

  getByName(name) {
    const index = this._document._backing.layers.findIndex((layer) => layer.name === name);
    if (index < 0) {
      throw new Error(`no layer named ${name}`);
    }
    return new ArtLayer(this._document, index);
  }

  [Symbol.iterator]() {
    let index = 0;
    const layers = this;
    return {
      next() {
        if (index >= layers.length) {
          return { done: true, value: undefined };
        }
        return { done: false, value: layers.item(index++) };
      },
    };
  }
}

export class Document {
  constructor({ width, height, name = "Untitled-1", backgroundColor = 0xffffffff }) {
    this._backing = new OpenshopDocument({ width, height, backgroundColor });
    this.name = name;
    this.artLayers = new ArtLayers(this);
    this.selection = new DocumentSelection(this);
  }

  get width() {
    return this._backing.width;
  }

  get height() {
    return this._backing.height;
  }

  get activeLayer() {
    return new ArtLayer(this, this._backing.activeLayer);
  }

  set activeLayer(layer) {
    this._backing.selectLayer(layer._index);
  }

  get backgroundLayer() {
    return new ArtLayer(this, 0);
  }

  get saved() {
    return !this._backing.dirty;
  }

  flatten() {
    this._backing.flatten();
  }

  mergeVisibleLayers() {
    for (let index = this._backing.layers.length - 1; index > 0; index--) {
      if (this._backing.layers[index].visible && this._backing.layers[index - 1].visible) {
        this._backing.mergeDown(index);
      }
    }
  }

  resizeImage(width, height) {
    this._backing.resizeImage(Math.round(width), Math.round(height));
  }

  resizeCanvas(width, height, anchor = AnchorPosition.MIDDLECENTER) {
    const w = Math.round(width);
    const h = Math.round(height);
    const dx = w - this._backing.width;
    const dy = h - this._backing.height;
    const offsets = {
      [AnchorPosition.TOPLEFT]: [0, 0],
      [AnchorPosition.TOPCENTER]: [Math.round(dx / 2), 0],
      [AnchorPosition.TOPRIGHT]: [dx, 0],
      [AnchorPosition.MIDDLELEFT]: [0, Math.round(dy / 2)],
      [AnchorPosition.MIDDLECENTER]: [Math.round(dx / 2), Math.round(dy / 2)],
      [AnchorPosition.MIDDLERIGHT]: [dx, Math.round(dy / 2)],
      [AnchorPosition.BOTTOMLEFT]: [0, dy],
      [AnchorPosition.BOTTOMCENTER]: [Math.round(dx / 2), dy],
      [AnchorPosition.BOTTOMRIGHT]: [dx, dy],
    };
    const offset = offsets[anchor];
    if (!offset) {
      throw new TypeError(`unknown anchor: ${anchor}`);
    }
    this._backing.resizeCanvas(w, h, offset[0], offset[1]);
  }

  crop({ left, top, right, bottom }) {
    this._backing.crop(left, top, right, bottom);
  }

  gradientFill({ x0, y0, x1, y1, startColor, endColor, type = GradientType.LINEAR }) {
    this._backing.gradientFill({ x0, y0, x1, y1, startColor, endColor, type });
  }

  saveAs(path) {
    if (String(path).toLowerCase().endsWith(".psd")) {
      this._backing.savePsd(path);
      return;
    }
    throw new Error("only .psd export is wired through the script facade");
  }

  commands() {
    return this._backing.commands.slice();
  }

  toJSON() {
    return this._backing.toJSON();
  }
}

export class Documents {
  constructor(application) {
    this._application = application;
    this._items = [];
  }

  get length() {
    return this._items.length;
  }

  add(width = 800, height = 600, _resolution = 72, name = undefined) {
    const document = new Document({
      width: Math.round(width),
      height: Math.round(height),
      name: name ?? `Untitled-${this._items.length + 1}`,
    });
    this._items.push(document);
    this._application._activeDocument = document;
    return document;
  }

  item(index) {
    if (index < 0 || index >= this._items.length) {
      throw new RangeError(`no document at index ${index}`);
    }
    return this._items[index];
  }

  getByName(name) {
    const document = this._items.find((item) => item.name === name);
    if (!document) {
      throw new Error(`no document named ${name}`);
    }
    return document;
  }

  [Symbol.iterator]() {
    return this._items[Symbol.iterator]();
  }
}

const batchPlayHandlers = {
  make(application, descriptor) {
    const target = descriptor._target?.[0]?._ref;
    if (target === "document") {
      const width = descriptor.width ?? 800;
      const height = descriptor.height ?? 600;
      return application.documents.add(width, height, 72, descriptor.name);
    }
    if (target === "layer") {
      return application.activeDocument.artLayers.add();
    }
    throw new Error(`batchPlay make: unsupported target ${target}`);
  },
  set(application, descriptor) {
    const layer = application.activeDocument.activeLayer;
    const to = descriptor.to ?? {};
    if (to.opacity !== undefined) {
      layer.opacity = to.opacity;
    }
    if (to.blendMode !== undefined) {
      layer.blendMode = to.blendMode;
    }
    if (to.name !== undefined) {
      layer.name = to.name;
    }
    if (to.visible !== undefined) {
      layer.visible = to.visible;
    }
    return layer;
  },
  delete(application) {
    application.activeDocument.activeLayer.remove();
    return undefined;
  },
  flattenImage(application) {
    application.activeDocument.flatten();
    return undefined;
  },
  selectAllPlusMinus(application) {
    application.activeDocument.selection.selectAll();
    return undefined;
  },
  set_selection(application, descriptor) {
    const to = descriptor.to ?? {};
    if (to._obj === "rectangle") {
      application.activeDocument.selection.selectRectangle(to);
    }
    return undefined;
  },
  crop(application, descriptor) {
    application.activeDocument.crop(descriptor.to ?? {});
    return undefined;
  },
  imageSize(application, descriptor) {
    application.activeDocument.resizeImage(descriptor.width, descriptor.height);
    return undefined;
  },
  canvasSize(application, descriptor) {
    application.activeDocument.resizeCanvas(descriptor.width, descriptor.height, descriptor.anchor ?? AnchorPosition.MIDDLECENTER);
    return undefined;
  },
  invert(application) {
    application.activeDocument.activeLayer.invert();
    return undefined;
  },
  desaturate(application) {
    application.activeDocument.activeLayer.desaturate();
    return undefined;
  },
  gaussianBlur(application, descriptor) {
    application.activeDocument.activeLayer.applyGaussianBlur(descriptor.radius ?? 1);
    return undefined;
  },
  brightnessEvent(application, descriptor) {
    application.activeDocument.activeLayer.adjustBrightnessContrast(descriptor.brightness ?? 0, descriptor.center ?? 0);
    return undefined;
  },
  hueSaturation(application, descriptor) {
    application.activeDocument.activeLayer.adjustHueSaturation(descriptor.hue ?? 0, descriptor.saturation ?? 0, descriptor.lightness ?? 0);
    return undefined;
  },
};

export class Application {
  constructor() {
    this.name = "OpenShop";
    this.version = "1.0.0";
    this.documents = new Documents(this);
    this._activeDocument = null;
  }

  get activeDocument() {
    if (!this._activeDocument) {
      throw new Error("no open documents");
    }
    return this._activeDocument;
  }

  set activeDocument(document) {
    this._activeDocument = document;
  }

  batchPlay(descriptors) {
    return descriptors.map((descriptor) => {
      const handler = batchPlayHandlers[descriptor._obj];
      if (!handler) {
        throw new Error(`batchPlay: unsupported descriptor ${descriptor._obj}`);
      }
      return handler(this, descriptor);
    });
  }
}

export const app = new Application();
