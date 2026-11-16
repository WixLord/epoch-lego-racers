// Headless recorder for our own Photoshop drawing script, sharing its geometry.
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const root = path.resolve(__dirname, '..');
const output = path.join(root, 'Assets', 'SpiderMan');
fs.mkdirSync(output, { recursive: true });
let active;
function File(p) { this.path = String(p); this.parent = { parent: root }; }
File.prototype.open = function() {};
File.prototype.write = function() {};
File.prototype.close = function() {};
File.prototype.toString = function() { return this.path; };
function Folder(p) { this.path = String(p); }
Folder.prototype.create = function() {};
Folder.prototype.toString = function() { return this.path; };
const app = { preferences: { rulerUnits: 0 }, displayDialogs: 0, documents: {
  add(w, h, dpi, name) {
    const doc = { width: w, height: h, name, layers: [] };
    doc.artLayers = { add() { const layer = { name: '', shapes: [] }; doc.layers.push(layer); doc.activeLayer = layer; return layer; } };
    doc.artLayers.add();
    let points;
    doc.selection = {
      select(p) { points = p; },
      fill(c) { doc.activeLayer.shapes.push({ color: c.rgb.hexValue, points }); },
      deselect() {}
    };
    doc.saveAs = function(file) {
      if (!file.path.endsWith('.png')) return;
      fs.writeFileSync(path.join(output, path.basename(file.path, '.png') + '.json'),
        JSON.stringify({ width: doc.width, height: doc.height, layers: doc.layers }, null, 2));
    };
    return active = doc;
  }
}};
vm.runInNewContext(fs.readFileSync(path.join(__dirname, 'Paint-SpiderMan.jsx'), 'utf8').replace(/^#target.*$/m, ''), {
  app, File, Folder, $: { fileName: path.join(__dirname, 'Paint-SpiderMan.jsx') },
  Units: { PIXELS: 1 }, DialogModes: { NO: 1 }, SelectionType: { REPLACE: 1 },
  ColorBlendMode: { NORMAL: 1 }, NewDocumentMode: { RGB: 1 }, DocumentFill: { TRANSPARENT: 1 },
  Extension: { LOWERCASE: 1 }, SolidColor: function() { this.rgb = {}; },
  PhotoshopSaveOptions: function() {}, PNGSaveOptions: function() {}
});
for (const name of ['mask', 'torso-front', 'torso-back']) {
  if (!fs.existsSync(path.join(output, name + '.json'))) throw new Error('Missing drawing: ' + name);
}
console.log('Recorded hand-authored mask and torso geometry.');
