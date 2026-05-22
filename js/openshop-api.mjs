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
      },
    ];
    this.activeLayer = 0;
    this.commands = [];
    this.dirty = false;
  }

  record(type, payload = {}) {
    const command = { type, ...payload };
    this.commands.push(command);
    return command;
  }

  addLayer(name = "Layer") {
    const index = this.layers.length;
    this.layers.push({ name, visible: true, locked: false, opacityPercent: 100 });
    this.activeLayer = index;
    this.dirty = true;
    return this.record("addLayer", { name, index });
  }

  insertLayer(index, name = "Layer") {
    this.layers.splice(index, 0, { name, visible: true, locked: false, opacityPercent: 100 });
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
    this.dirty = true;
    return this.record("mergeDown", { index });
  }

  flatten() {
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

  toJSON() {
    return {
      width: this.width,
      height: this.height,
      backgroundColor: this.backgroundColor,
      activeLayer: this.activeLayer,
      layers: this.layers,
      commands: this.commands,
      dirty: this.dirty,
    };
  }
}
