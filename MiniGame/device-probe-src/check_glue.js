// Offline regression: the lowered Emscripten glue still loads real WASM and completes its suspending calls.
// Usage: node check_glue.js <package dir>
const fs = require('fs');
const path = require('path');

const packageDir = path.resolve(process.argv[2]);
let failed = 0;

function report(name, ok, detail) {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${detail === undefined ? '' : ` (${detail})`}`);
  if (!ok) failed++;
}

function load(glue, wasm, options) {
  const factory = require(path.join(packageDir, glue));
  return factory(Object.assign({
    instantiateWasm(imports, receive) {
      WebAssembly.instantiate(fs.readFileSync(path.join(packageDir, wasm)), imports).then(({ instance }) => receive(instance));
      return {};
    },
  }, options));
}

(async () => {
  for (const file of ['dawn-probe.js']) {
    const source = fs.readFileSync(path.join(packageDir, file), 'utf8');
    report(`${file} has no ?. syntax left`, !/[\w\])]\?\.[\w[(]/.test(source.replace(/"[^"\n]*"|'[^'\n]*'|`[^`]*`/g, '""')));
  }

  // Node 21+ defines a read-only navigator, so a plain assignment would be ignored.
  const setNavigator = value => Object.defineProperty(globalThis, 'navigator', { value, configurable: true, writable: true });

  // No adapter is offered, so the triangle stops at stage 2 after a real Future wait through the lowered BigInt paths.
  setNavigator({
    gpu: { requestAdapter: async () => null, getPreferredCanvasFormat: () => 'bgra8unorm', wgslLanguageFeatures: new Set() },
  });
  const dawn = await load('dawn-probe.js', 'dawn-probe.wasm', { canvas: { width: 8, height: 8 } });
  const code = await dawn.ccall('drawDawnTriangle', 'number', ['number', 'number'], [8, 8], { async: true });
  report('dawn glue waits on a Future and reports the adapter stage', code === 2, code);
  process.exit(failed ? 1 : 0);
})().catch(error => {
  console.error('FAIL', error);
  process.exit(1);
});
