export const Tools = Object.freeze({
  brush: "brush",
  eraser: "eraser",
  line: "line",
  rect: "rect",
  filledRect: "filledRect",
  ellipse: "ellipse",
  filledEllipse: "filledEllipse",
});

export const BrushShapes = Object.freeze({
  round: "round",
  square: "square",
  diamond: "diamond",
});

export const BlendModes = Object.freeze({
  normal: "normal",
  multiply: "multiply",
  screen: "screen",
  overlay: "overlay",
  softLight: "softLight",
  hardLight: "hardLight",
  darken: "darken",
  lighten: "lighten",
  colorDodge: "colorDodge",
  colorBurn: "colorBurn",
  linearDodge: "linearDodge",
  linearBurn: "linearBurn",
  difference: "difference",
  exclusion: "exclusion",
});

export const SelectionOps = Object.freeze({
  replace: "replace",
  add: "add",
  subtract: "subtract",
});

export const GradientTypes = Object.freeze({
  linear: "linear",
  radial: "radial",
});

export const VfxBrushes = Object.freeze({
  softRound: "softRound",
  airbrush: "airbrush",
  splatter: "splatter",
  glow: "glow",
  sparkle: "sparkle",
  smoke: "smoke",
});

export class OpenshopDocument {
  constructor({ width, height, backgroundColor = 0xffffffff }) {
    if (!Number.isInteger(width) || !Number.isInteger(height) || width <= 0 || height <= 0) {
      throw new TypeError("width and height must be positive integers");
    }
    this.width = width;
    this.height = height;
    this.backgroundColor = backgroundColor >>> 0;
    this.layers = [
      {
        name: "Background",
        visible: true,
        locked: false,
        opacityPercent: 100,
        blendMode: BlendModes.normal,
      },
    ];
    this.activeLayer = 0;
    this.commands = [];
    this.dirty = false;
    this.hasSelection = false;
  }

  record(type, payload = {}) {
    const command = { ...payload, type };
    this.commands.push(command);
    return command;
  }

  addLayer(name = "Layer") {
    const index = this.layers.length;
    this.layers.push({ name, visible: true, locked: false, opacityPercent: 100, blendMode: BlendModes.normal });
    this.activeLayer = index;
    this.dirty = true;
    return this.record("addLayer", { name, index });
  }

  insertLayer(index, name = "Layer") {
    this.layers.splice(index, 0, { name, visible: true, locked: false, opacityPercent: 100, blendMode: BlendModes.normal });
    this.activeLayer = index;
    this.dirty = true;
    return this.record("insertLayer", { index, name });
  }

  selectLayer(index) {
    this.activeLayer = index;
    return this.record("selectLayer", { index });
  }

  duplicateLayer(index = this.activeLayer, name = "Layer Copy") {
    const source = this.layers[index];
    this.layers.splice(index + 1, 0, { ...source, name });
    this.activeLayer = index + 1;
    this.dirty = true;
    return this.record("duplicateLayer", { index, name });
  }

  deleteLayer(index = this.activeLayer) {
    this.layers.splice(index, 1);
    this.activeLayer = Math.max(0, Math.min(this.activeLayer, this.layers.length - 1));
    this.dirty = true;
    return this.record("deleteLayer", { index });
  }

  moveLayer(index, direction) {
    this.dirty = true;
    return this.record("moveLayer", { index, direction });
  }

  setLayerVisible(index, visible) {
    this.layers[index].visible = Boolean(visible);
    this.dirty = true;
    return this.record("setLayerVisible", { index, visible: Boolean(visible) });
  }

  setLayerLocked(index, locked) {
    this.layers[index].locked = Boolean(locked);
    this.dirty = true;
    return this.record("setLayerLocked", { index, locked: Boolean(locked) });
  }

  setLayerOpacity(index, opacityPercent) {
    this.layers[index].opacityPercent = opacityPercent;
    this.dirty = true;
    return this.record("setLayerOpacity", { index, opacityPercent });
  }

  setLayerBlendMode(index, blendMode) {
    if (!Object.values(BlendModes).includes(blendMode)) {
      throw new TypeError(`unknown blend mode: ${blendMode}`);
    }
    this.layers[index].blendMode = blendMode;
    this.dirty = true;
    return this.record("setLayerBlendMode", { index, blendMode });
  }

  renameLayer(index, name) {
    this.layers[index].name = name;
    this.dirty = true;
    return this.record("renameLayer", { index, name });
  }

  clearLayer(index = this.activeLayer) {
    this.dirty = true;
    return this.record("clearLayer", { index });
  }

  mergeDown(index = this.activeLayer) {
    if (index > 0 && index < this.layers.length) {
      this.layers.splice(index, 1);
      this.layers[index - 1].opacityPercent = 100;
      this.layers[index - 1].blendMode = BlendModes.normal;
      this.activeLayer = Math.min(Math.max(0, index - 1), this.layers.length - 1);
    }
    this.dirty = true;
    return this.record("mergeDown", { index });
  }

  flatten() {
    this.layers = [
      {
        ...this.layers[0],
        visible: true,
        locked: false,
        opacityPercent: 100,
        blendMode: BlendModes.normal,
      },
    ];
    this.activeLayer = 0;
    this.dirty = true;
    return this.record("flatten");
  }

  drawStroke({ x0, y0, x1, y1, radius, color, shape = BrushShapes.round }) {
    this.dirty = true;
    return this.record("drawStroke", { x0, y0, x1, y1, radius, color: color >>> 0, shape });
  }

  eraseStroke({ x0, y0, x1, y1, radius, shape = BrushShapes.round }) {
    this.dirty = true;
    return this.record("eraseStroke", { x0, y0, x1, y1, radius, shape });
  }

  drawShape({ tool, x0, y0, x1, y1, radius = 1, color }) {
    this.dirty = true;
    return this.record("drawShape", { tool, x0, y0, x1, y1, radius, color: color >>> 0 });
  }

  fill({ x, y, color }) {
    this.dirty = true;
    return this.record("fill", { x, y, color: color >>> 0 });
  }

  flipActiveHorizontal() {
    this.dirty = true;
    return this.record("flipActiveHorizontal");
  }

  flipActiveVertical() {
    this.dirty = true;
    return this.record("flipActiveVertical");
  }

  rotateActive180() {
    this.dirty = true;
    return this.record("rotateActive180");
  }

  invertActiveRgb() {
    this.dirty = true;
    return this.record("invertActiveRgb");
  }

  translateActive(dx, dy) {
    this.dirty = true;
    return this.record("translateActive", { dx, dy });
  }

  drawVfxStroke({ x0, y0, x1, y1, radius, color, preset = VfxBrushes.softRound, seed = 0 }) {
    if (!Object.values(VfxBrushes).includes(preset)) {
      throw new TypeError(`unknown vfx brush: ${preset}`);
    }
    this.dirty = true;
    return this.record("drawVfxStroke", { x0, y0, x1, y1, radius, color: color >>> 0, preset, seed: seed >>> 0 });
  }

  adjustBrightnessContrast(brightness, contrast) {
    this.dirty = true;
    return this.record("adjustBrightnessContrast", { brightness, contrast });
  }

  adjustHueSaturation(hue, saturation, lightness = 0) {
    this.dirty = true;
    return this.record("adjustHueSaturation", { hue, saturation, lightness });
  }

  adjustLevels({ inBlack = 0, inWhite = 255, gamma = 1.0, outBlack = 0, outWhite = 255 } = {}) {
    this.dirty = true;
    return this.record("adjustLevels", { inBlack, inWhite, gamma, outBlack, outWhite });
  }

  desaturateActive() {
    this.dirty = true;
    return this.record("desaturateActive");
  }

  posterizeActive(levels) {
    this.dirty = true;
    return this.record("posterizeActive", { levels });
  }

  thresholdActive(level) {
    this.dirty = true;
    return this.record("thresholdActive", { level });
  }

  blurActive(radius) {
    this.dirty = true;
    return this.record("blurActive", { radius });
  }

  sharpenActive(amountPercent) {
    this.dirty = true;
    return this.record("sharpenActive", { amountPercent });
  }

  selectAll() {
    this.hasSelection = true;
    return this.record("selectAll");
  }

  deselect() {
    this.hasSelection = false;
    return this.record("deselect");
  }

  invertSelection() {
    this.hasSelection = true;
    return this.record("invertSelection");
  }

  selectRect(x0, y0, x1, y1, op = SelectionOps.replace) {
    if (!Object.values(SelectionOps).includes(op)) {
      throw new TypeError(`unknown selection op: ${op}`);
    }
    this.hasSelection = true;
    return this.record("selectRect", { x0, y0, x1, y1, op });
  }

  selectEllipse(x0, y0, x1, y1, op = SelectionOps.replace) {
    if (!Object.values(SelectionOps).includes(op)) {
      throw new TypeError(`unknown selection op: ${op}`);
    }
    this.hasSelection = true;
    return this.record("selectEllipse", { x0, y0, x1, y1, op });
  }

  magicWand(x, y, tolerance = 32, op = SelectionOps.replace) {
    if (!Object.values(SelectionOps).includes(op)) {
      throw new TypeError(`unknown selection op: ${op}`);
    }
    this.hasSelection = true;
    return this.record("magicWand", { x, y, tolerance, op });
  }

  featherSelection(radius) {
    return this.record("featherSelection", { radius });
  }

  gradientFill({ x0, y0, x1, y1, startColor, endColor, type = GradientTypes.linear }) {
    if (!Object.values(GradientTypes).includes(type)) {
      throw new TypeError(`unknown gradient type: ${type}`);
    }
    this.dirty = true;
    return this.record("gradientFill", { x0, y0, x1, y1, startColor: startColor >>> 0, endColor: endColor >>> 0, gradientType: type });
  }

  resizeImage(width, height) {
    if (!Number.isInteger(width) || !Number.isInteger(height) || width <= 0 || height <= 0) {
      throw new TypeError("width and height must be positive integers");
    }
    this.width = width;
    this.height = height;
    this.hasSelection = false;
    this.dirty = true;
    return this.record("resizeImage", { width, height });
  }

  resizeCanvas(width, height, offsetX = 0, offsetY = 0) {
    if (!Number.isInteger(width) || !Number.isInteger(height) || width <= 0 || height <= 0) {
      throw new TypeError("width and height must be positive integers");
    }
    this.width = width;
    this.height = height;
    this.hasSelection = false;
    this.dirty = true;
    return this.record("resizeCanvas", { width, height, offsetX, offsetY });
  }

  crop(x0, y0, x1, y1) {
    const left = Math.max(0, Math.min(x0, x1));
    const top = Math.max(0, Math.min(y0, y1));
    const right = Math.min(this.width - 1, Math.max(x0, x1));
    const bottom = Math.min(this.height - 1, Math.max(y0, y1));
    if (right < left || bottom < top) {
      throw new RangeError("crop region outside canvas");
    }
    this.width = right - left + 1;
    this.height = bottom - top + 1;
    this.hasSelection = false;
    this.dirty = true;
    return this.record("crop", { x0, y0, x1, y1 });
  }

  savePsd(path) {
    return this.record("savePsd", { path });
  }

  loadPsd(path) {
    this.dirty = true;
    return this.record("loadPsd", { path });
  }

  toJSON() {
    return {
      width: this.width,
      height: this.height,
      backgroundColor: this.backgroundColor,
      activeLayer: this.activeLayer,
      layers: this.layers,
      commands: this.commands,
      dirty: this.dirty,
      hasSelection: this.hasSelection,
    };
  }
}
