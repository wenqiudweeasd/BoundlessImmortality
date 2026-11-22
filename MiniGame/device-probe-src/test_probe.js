// Offline regression for device-probe/game.js: runs it against a fake wx host with real WebAssembly.
// Usage: node test_probe.js <package dir>
// Covers the two paths that do not need a GPU: no WebGPU at all, and a WebGPU device that passes the capability
// checks while the Emscripten modules fail cleanly. This is offline evidence only; it says nothing about phones.
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const Module = require('module');

const packageDir = path.resolve(process.argv[2]);

function createHost({ gpu }) {
  const storage = new Map();
  const logs = [];
  const painted = [];
  const loadingTitles = [];
  let touchEnd = null;
  let actionSheet;
  let resolveDone;
  const done = new Promise(resolve => { resolveDone = resolve; });

  const fakeGl = {
    VERSION: 1, SHADING_LANGUAGE_VERSION: 2, VENDOR: 3, RENDERER: 4, MAX_TEXTURE_SIZE: 5, MAX_UNIFORM_BLOCK_SIZE: 6,
    MAX_COLOR_ATTACHMENTS: 7,
    getExtension: () => null,
    getParameter: key => ({ 1: 'WebGL 2.0', 2: 'GLSL ES 3.0', 3: 'V', 4: 'R', 5: 8192, 6: 65536, 7: 4 }[key]),
    getSupportedExtensions: () => ['EXT_color_buffer_float'],
  };
  const context2d = { fillRect() {}, fillText: text => painted.push(text), set fillStyle(_) {}, set font(_) {} };
  const makeCanvas = () => ({
    width: 0,
    height: 0,
    getContext: type => {
      if (type === 'webgl2') return fakeGl;
      if (type === '2d') return context2d;
      if (type === 'webgpu') return gpu ? {} : null;
      return null;
    },
  });

  const wx = {
    createCanvas: makeCanvas,
    getWindowInfo: () => ({ windowWidth: 390, windowHeight: 844, pixelRatio: 3 }),
    getDeviceInfo: () => ({ platform: 'android', brand: 'Fake', model: 'Phone', system: 'Android 14', memorySize: 8192, benchmarkLevel: 30, cpuType: 'x' }),
    getAppBaseInfo: () => ({ version: '8.0.60', SDKVersion: '3.18.0' }),
    getDeviceBenchmarkInfo: ({ success }) => success({ benchmarkLevel: 40, modelLevel: 3 }),
    ...(gpu ? { getGPU: () => gpu.object } : {}),
    showLoading: ({ title }) => loadingTitles.push(title),
    hideLoading() {},
    showModal: options => { resolveDone(options); },
    showActionSheet: options => { actionSheet = options; },
    setClipboardData() {},
    onTouchEnd: handler => { touchEnd = handler; },
    getStorageSync: key => storage.get(key) || '',
    setStorageSync: (key, value) => storage.set(key, value),
    removeStorageSync: key => storage.delete(key),
  };

  const WXWebAssembly = {
    instantiate: async (relative, imports) => {
      const bytes = fs.readFileSync(path.join(packageDir, relative));
      return WebAssembly.instantiate(bytes, imports);
    },
  };

  const sandbox = {
    wx,
    WXWebAssembly,
    console: { log: (...args) => logs.push(args.join(' ')), error: (...args) => logs.push(args.join(' ')) },
    setTimeout,
    clearTimeout,
    Date,
    Promise,
    JSON,
    Number,
    String,
    Boolean,
    Array,
    Object,
    Math,
    Error,
    BigInt,
    WebAssembly: undefined,
    require: request => {
      if (request.startsWith('./')) throw new Error(`stubbed require ${request}`);
      return Module.createRequire(__filename)(request);
    },
  };
  const counters = { devices: 0 };
  if (gpu) {
    // Phones have no navigator at all: the GPU object only comes from wx.getGPU.
    gpu.object = {
      requestAdapter: async () => ({
        info: { vendor: 'fake', architecture: 'a', device: 'd', description: 'mock' },
        features: new Set(['texture-compression-astc']),
        limits: {
          maxBindGroups: 4, maxStorageBuffersPerShaderStage: gpu.storageBuffers, maxStorageTexturesPerShaderStage: 8,
          maxSampledTexturesPerShaderStage: 16, maxUniformBuffersPerShaderStage: 12, maxBufferSize: 1 << 30,
          maxStorageBufferBindingSize: 1 << 28, maxTextureDimension2D: 8192, maxColorAttachments: 8,
        },
        requestDevice: async () => { counters.devices++; return { destroy() {} }; },
      }),
    };
  }
  sandbox.GameGlobal = sandbox;
  return {
    sandbox, logs, painted, loadingTitles, done, counters,
    selectAction: tapIndex => {
      touchEnd();
      return actionSheet.success({ tapIndex });
    },
  };
}

async function runCase(name, options, assertions) {
  const host = createHost(options);
  const context = vm.createContext(host.sandbox);
  const source = fs.readFileSync(path.join(packageDir, 'game.js'), 'utf8');
  vm.runInContext(source, context, { filename: 'game.js' });
  const modal = await Promise.race([
    host.done,
    new Promise((_, reject) => setTimeout(() => reject(new Error('probe never finished')), 30000)),
  ]);
  const line = host.logs.find(text => text.startsWith('[UE-WASM-DEVICE]'));
  if (!line) throw new Error(`${name}: no report line`);
  const report = JSON.parse(line.slice('[UE-WASM-DEVICE] '.length));
  await assertions(report, modal, host);
  console.log(`ok   ${name}`);
}

function expect(condition, message) {
  if (!condition) throw new Error(message);
}

(async () => {
  await runCase('no WebGPU: falls back to the 2D report', { gpu: null }, async (report, modal, host) => {
    expect(report.wasm.simd128.ok, 'simd128 should pass in Node');
    expect(report.wasm['i64-bigint'].ok, 'i64-bigint should pass in Node');
    const wasmSteps = host.logs.filter(line => line.startsWith('[UE-WASM-STEP] '));
    expect(wasmSteps.length === Object.keys(report.wasm).length - 1, 'each automatic WASM feature should log its start');
    expect(report.wasm.memory64.skipped === true, 'memory64 should be marked as a manual test');
    expect(!wasmSteps.includes('[UE-WASM-STEP] memory64'), 'memory64 must not instantiate during startup');
    expect(host.loadingTitles[0] === '探针：设备信息', 'loading should show when probe code has started');
    expect(host.loadingTitles.includes('WASM threads'), 'WASM loading should name the active feature');
    expect(host.loadingTitles.includes('探针：WebGPU'), 'WebGPU loading should be visible on the phone');
    expect(report.webgl2.ok, 'WebGL2 fake should pass');
    expect(report.webgpu.source === null, 'no GPU source expected');
    expect(report.webgpu.environment.wechatOk === false, 'WeChat 8.0.60 is below the official minimum');
    expect(report.webgpu.environment.baseLibraryOk === true, 'base library 3.18.0 meets the official minimum');
    expect(report.dawn.detail.startsWith('skipped'), 'dawn should be skipped');
    expect(report.verdict.canRunUEWebGPUFork === false, 'route must be reported as unavailable');
    expect(report.verdict.wasmFeaturesForUE === true, 'wasm features should be reported as fine');
    expect(host.painted.some(text => text.includes('WebGPU 设备 不可用')), '2D report should be painted');
    expect(modal.content.includes('UE WebGPU 路线 不可用'), 'modal must state the verdict');
    expect(modal.content.includes('微信≥8.0.79'), 'modal must name the version the phone needs');
    await host.selectAction(2);
    const updatedReport = JSON.parse(host.logs.at(-1).slice('[UE-WASM-DEVICE] '.length));
    expect(updatedReport.wasm.memory64.ok === true, 'memory64 should run when selected from the menu');
    expect(updatedReport.verdict.wasmFeaturesForUE === true, 'manual memory64 should not change the UE verdict');
  });

  await runCase('wx.getGPU follows the official constraints', { gpu: { storageBuffers: 8 } }, (report, modal, host) => {
    expect(report.webgpu.source === 'wx.getGPU', 'the GPU must come from wx.getGPU');
    expect(report.webgpu.adapter === true, 'mock adapter should be read');
    expect(host.counters.devices === 0, 'the capability check must not create the single allowed device');
    expect(report.webgl2.skipped === true, 'WebGL must not be touched when WebGPU will be used');
    expect(host.sandbox.navigator && host.sandbox.navigator.gpu, 'the Emscripten bindings need navigator.gpu');
    expect(report.verdict.webgl2 === 'not tested', 'an untested WebGL2 must not read as a failure');
    expect(report.verdict.webgpuBudgetForUE === false, 'budget 8 must fail the UE requirement of 10');
    expect(report.verdict.canRunUEWebGPUFork === false, 'route must be reported as unavailable');
    expect(/stubbed require/.test(report.dawn.detail), 'dawn failure should be captured, not thrown');
    expect(report.asyncify.ok === false, 'asyncify is only proven by a finished Dawn run');
  });
  console.log('all probe cases passed');
})().catch(error => {
  console.error('FAIL', error);
  process.exit(1);
});
