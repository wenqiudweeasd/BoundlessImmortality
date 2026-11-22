// Offline check: every generated feature module must validate and return the documented value in Node.
const fs = require('fs');
const path = require('path');
const dir = process.argv[2];
let failed = 0;
for (const file of fs.readdirSync(dir).filter(name => name.endsWith('.wasm')).sort()) {
  const name = path.basename(file, '.wasm');
  try {
    const bytes = fs.readFileSync(path.join(dir, file));
    const { exports } = new WebAssembly.Instance(new WebAssembly.Module(bytes), {});
    let value;
    if (name === 'memory-probe') {
      const old = exports.grow(16);
      exports.touch(16, 16);
      value = old === 16 && exports.memory.buffer.byteLength === 32 * 65536 ? 42 : `old=${old}`;
    } else if (name === 'i64-bigint') {
      value = Number(exports.f(BigInt(42)));
    } else {
      value = exports.f();
    }
    console.log(value === 42 ? 'ok  ' : 'BAD ', name, value);
    if (value !== 42) failed++;
  } catch (error) {
    console.log('FAIL', name, String(error));
    failed++;
  }
}
process.exit(failed ? 1 : 0);
