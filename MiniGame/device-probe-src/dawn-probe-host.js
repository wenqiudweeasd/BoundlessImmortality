var document = {
  querySelector(selector) {
    return selector === '#canvas' ? Module.canvas : null;
  },
};
var window = globalThis.window ?? {};
