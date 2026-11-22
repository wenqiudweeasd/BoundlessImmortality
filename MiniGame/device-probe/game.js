'use strict';

/*
 * Real-device capability probe for the UE -> WASM -> WeChat port.
 *
 * Runs in one scan what the port depends on: the WebAssembly features the UE build is compiled with, the rendering
 * APIs the host really exposes, Asyncify over a GPU readback and a Dawn triangle. The device simulator in WeChat
 * DevTools is Chromium based, so only a phone run counts as evidence.
 *
 * Output: console line starting with [UE-WASM-DEVICE], and the clipboard after tapping the screen.
 */

const STEP_TIMEOUT_MS = 12000;
const MEMORY_KEY = 'ue-probe-memory-progress';
const MEMORY_STEP_PAGES = 1024; // 64 MiB
const WASM_FEATURES = [
  'simd128', 'bulk-memory', 'sign-ext', 'nontrapping-fptoint', 'multi-value',
  'tail-call', 'exceptions', 'threads', 'i64-bigint',
];
const GPU_LIMITS = [
  'maxBindGroups', 'maxStorageBuffersPerShaderStage', 'maxStorageTexturesPerShaderStage',
  'maxSampledTexturesPerShaderStage', 'maxUniformBuffersPerShaderStage', 'maxBufferSize',
  'maxStorageBufferBindingSize', 'maxTextureDimension2D', 'maxColorAttachments',
];
// WebGPURHI refuses to start below this budget (FWebGPUShaderHeader::FLimits, ADR-003).
const UE_MIN_STORAGE_BUFFERS = 10;
const UE_MIN_STORAGE_TEXTURES = 8;
// Official requirements of wx.getGPU: https://developers.weixin.qq.com/minigame/dev/guide/performance/WebGPU.html
const WEBGPU_MIN_WECHAT = '8.0.79';
const WEBGPU_MIN_BASE_LIBRARY = '3.17.3';
const WEBGPU_MIN_ANDROID = 10;

const screen = wx.createCanvas();
const windowInfo = wx.getWindowInfo();
screen.width = Math.round(windowInfo.windowWidth * windowInfo.pixelRatio);
screen.height = Math.round(windowInfo.windowHeight * windowInfo.pixelRatio);

const report = {
  probe: 'ue-wasm-device-probe',
  schema: 1,
  startedAt: new Date().toISOString(),
  device: {},
  wechat: {},
  host: {},
  wasm: {},
  webgl2: {},
  webgpu: {},
  asyncify: {},
  dawn: {},
  memory: {},
  verdict: {},
};

let finished = false;

function errorText(error) {
  return error && error.message ? String(error.message) : String(error);
}

function delay(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function withTimeout(promise, label) {
  let timer;
  return Promise.race([
    promise,
    new Promise((_, reject) => {
      timer = setTimeout(() => reject(new Error(`${label}: ${STEP_TIMEOUT_MS / 1000}s timeout`)), STEP_TIMEOUT_MS);
    }),
  ]).then(
    value => { clearTimeout(timer); return value; },
    error => { clearTimeout(timer); throw error; },
  );
}

function setStatus(title) {
  wx.showLoading({ title, mask: false });
}

function collectDevice() {
  const device = wx.getDeviceInfo();
  const base = wx.getAppBaseInfo();
  report.device = {
    platform: device.platform,
    brand: device.brand,
    model: device.model,
    system: device.system,
    memorySizeMB: device.memorySize,
    benchmarkLevel: device.benchmarkLevel,
    cpuType: device.cpuType,
    windowWidth: windowInfo.windowWidth,
    windowHeight: windowInfo.windowHeight,
    pixelRatio: windowInfo.pixelRatio,
  };
  report.wechat = {
    version: base.version,
    SDKVersion: base.SDKVersion,
    iOSHighPerformance: Boolean(GameGlobal.isIOSHighPerformanceMode),
    iOSHighPerformancePlus: Boolean(GameGlobal.isIOSHighPerformanceModePlus),
  };
}

function collectBenchmark() {
  if (typeof wx.getDeviceBenchmarkInfo !== 'function') return Promise.resolve();
  return withTimeout(new Promise(resolve => {
    wx.getDeviceBenchmarkInfo({
      success: info => {
        report.device.benchmarkScore = info.benchmarkLevel;
        report.device.modelLevel = info.modelLevel;
        resolve();
      },
      fail: resolve,
    });
  }), 'benchmark').catch(() => {});
}

function collectHost() {
  const interesting = /gpu|webassembly|wasm|offscreen|sharedarray|worker/i;
  const names = target => Object.getOwnPropertyNames(target).filter(name => interesting.test(name)).sort();
  report.host = {
    wxApis: names(wx),
    globals: names(GameGlobal),
    types: {
      WXWebAssembly: typeof WXWebAssembly,
      WebAssembly: typeof WebAssembly,
      navigator: typeof navigator,
      document: typeof document,
      BigInt: typeof BigInt,
      SharedArrayBuffer: typeof SharedArrayBuffer,
      Atomics: typeof Atomics,
      OffscreenCanvas: typeof OffscreenCanvas,
      requestAnimationFrame: typeof requestAnimationFrame,
      XMLHttpRequest: typeof XMLHttpRequest,
      fetch: typeof fetch,
    },
    // Blocking-call mechanisms: JSPI needs these two functions; the WeChat build falls back to Asyncify.
    jspi: typeof WebAssembly !== 'undefined' &&
      typeof WebAssembly.Suspending === 'function' &&
      typeof WebAssembly.promising === 'function',
  };
}

async function checkWasmFeature(name) {
  try {
    setStatus(`WASM ${name}`);
    console.log('[UE-WASM-STEP]', name);
    const started = Date.now();
    const { instance } = await withTimeout(WXWebAssembly.instantiate(`features/${name}.wasm`, {}), name);
    const value = name === 'i64-bigint' ? Number(instance.exports.f(BigInt(42))) : instance.exports.f();
    report.wasm[name] = value === 42
      ? { ok: true, ms: Date.now() - started }
      : { ok: false, detail: `returned ${value}` };
  } catch (error) {
    report.wasm[name] = { ok: false, detail: errorText(error) };
  }
}

function checkWebGL2() {
  try {
    const gl = wx.createCanvas().getContext('webgl2');
    if (!gl) {
      report.webgl2 = { ok: false, detail: 'getContext returned null' };
      return;
    }
    const debug = gl.getExtension('WEBGL_debug_renderer_info');
    report.webgl2 = {
      ok: true,
      version: gl.getParameter(gl.VERSION),
      shadingLanguage: gl.getParameter(gl.SHADING_LANGUAGE_VERSION),
      vendor: gl.getParameter(debug ? debug.UNMASKED_VENDOR_WEBGL : gl.VENDOR),
      renderer: gl.getParameter(debug ? debug.UNMASKED_RENDERER_WEBGL : gl.RENDERER),
      maxTextureSize: gl.getParameter(gl.MAX_TEXTURE_SIZE),
      maxUniformBlockSize: gl.getParameter(gl.MAX_UNIFORM_BLOCK_SIZE),
      maxColorAttachments: gl.getParameter(gl.MAX_COLOR_ATTACHMENTS),
      extensions: gl.getSupportedExtensions(),
    };
  } catch (error) {
    report.webgl2 = { ok: false, detail: errorText(error) };
  }
}

function compareVersions(a, b) {
  const left = String(a).split('.').map(Number);
  const right = String(b).split('.').map(Number);
  for (let i = 0; i < Math.max(left.length, right.length); i++) {
    const delta = (left[i] || 0) - (right[i] || 0);
    if (delta) return delta;
  }
  return 0;
}

/** wx.getGPU is the official entry on phones; the Chromium-based simulator also has navigator.gpu. */
function findGpu() {
  if (typeof wx.getGPU === 'function') {
    const gpu = wx.getGPU();
    if (gpu) return { gpu, source: 'wx.getGPU' };
  }
  if (typeof navigator !== 'undefined' && navigator.gpu) return { gpu: navigator.gpu, source: 'navigator.gpu' };
  return null;
}

function checkWebGPUEnvironment() {
  const androidVersion = parseInt(String(report.device.system).replace(/[^0-9.]/g, ''), 10);
  const isAndroid = report.device.platform === 'android';
  report.webgpu.environment = {
    isAndroid,
    androidOk: isAndroid && androidVersion >= WEBGPU_MIN_ANDROID,
    wechatOk: compareVersions(report.wechat.version, WEBGPU_MIN_WECHAT) >= 0,
    baseLibraryOk: compareVersions(report.wechat.SDKVersion, WEBGPU_MIN_BASE_LIBRARY) >= 0,
  };
}

async function checkWebGPU() {
  const out = { wxGetGPU: typeof wx.getGPU === 'function', source: null, adapter: false, device: false };
  report.webgpu = out;
  checkWebGPUEnvironment();
  const found = findGpu();
  if (!found) return;
  out.source = found.source;
  // The Emscripten bindings look for navigator.gpu, which phones only expose through wx.getGPU.
  if (typeof navigator === 'undefined') GameGlobal.navigator = { gpu: found.gpu };
  else if (!navigator.gpu) navigator.gpu = found.gpu;
  try {
    const adapter = await withTimeout(found.gpu.requestAdapter(), 'requestAdapter');
    if (!adapter) {
      out.detail = 'requestAdapter() returned null';
      return;
    }
    out.adapter = true;
    const info = adapter.info || {};
    out.adapterInfo = {
      vendor: info.vendor, architecture: info.architecture, device: info.device, description: info.description,
    };
    out.features = Array.from(adapter.features || []).sort();
    out.limits = {};
    for (const key of GPU_LIMITS) out.limits[key] = adapter.limits[key];
    // No device here: WeChat allows one per game and only after context.configure, so the Dawn step creates it.
  } catch (error) {
    out.detail = errorText(error);
  }
}

function loadEmscriptenModule(factoryPath, wasmPath, options) {
  const factory = require(factoryPath);
  return new Promise((resolve, reject) => {
    factory(Object.assign({
      instantiateWasm(imports, receive) {
        WXWebAssembly.instantiate(wasmPath, imports)
          .then(({ instance }) => receive(instance))
          .catch(reject);
        return {};
      },
    }, options)).then(resolve, reject);
  });
}

async function runDawnTriangle() {
  const stages = [
    'instance', 'adapter future', 'device future', 'canvas surface',
    'surface capabilities', 'surface texture', 'GPU submission future',
  ];
  try {
    const started = Date.now();
    const module = await withTimeout(loadEmscriptenModule('./dawn-probe.js', 'dawn-probe.wasm', { canvas: screen }), 'dawn load');
    const code = await withTimeout(
      module.ccall('drawDawnTriangle', 'number', ['number', 'number'], [screen.width, screen.height], { async: true }),
      'dawn triangle');
    report.dawn = code === 42
      ? { ok: true, ms: Date.now() - started, detail: 'expect a cyan triangle on dark blue' }
      : { ok: false, detail: `stage ${code} failed: ${stages[code - 1] || 'unknown'}` };
  } catch (error) {
    report.dawn = { ok: false, detail: errorText(error) };
  }
}

function buildVerdict() {
  const feature = name => Boolean(report.wasm[name] && report.wasm[name].ok);
  const limits = report.webgpu.limits || {};
  const compute = feature('simd128') && feature('bulk-memory') && feature('sign-ext') &&
    feature('nontrapping-fptoint') && feature('i64-bigint');
  const gpuBudget = limits.maxStorageBuffersPerShaderStage >= UE_MIN_STORAGE_BUFFERS &&
    limits.maxStorageTexturesPerShaderStage >= UE_MIN_STORAGE_TEXTURES;
  report.verdict = {
    wasmFeaturesForUE: compute,
    webgl2: report.webgl2.skipped ? 'not tested' : Boolean(report.webgl2.ok),
    webgpuDevice: Boolean(report.webgpu.device),
    webgpuBudgetForUE: Boolean(report.webgpu.adapter && gpuBudget),
    asyncifyOverGpu: Boolean(report.asyncify.ok),
    dawnTriangle: Boolean(report.dawn.ok),
    canRunUEWebGPUFork: Boolean(compute && report.webgpu.device && gpuBudget && report.dawn.ok),
  };
}

function summaryLines() {
  const mark = value => (value ? '通过' : '不可用');
  const verdict = report.verdict;
  const environment = report.webgpu.environment;
  const lines = [
    `${report.device.platform} ${report.device.model}`,
    `微信 ${report.wechat.version} 基础库 ${report.wechat.SDKVersion}`,
    `WASM 特性 ${mark(verdict.wasmFeaturesForUE)}`,
    verdict.webgl2 === 'not tested' ? 'WebGL2 未测（不能与 WebGPU 共存）' : `WebGL2 ${mark(verdict.webgl2)}`,
    `WebGPU 设备 ${mark(verdict.webgpuDevice)}`,
    `Dawn 三角形 ${mark(verdict.dawnTriangle)}`,
    `UE WebGPU 路线 ${mark(verdict.canRunUEWebGPUFork)}`,
  ];
  if (report.device.platform !== 'devtools' && environment) {
    if (!environment.isAndroid) {
      lines.push('官方 WebGPU 目前仅支持 Android');
    } else if (!environment.androidOk || !environment.wechatOk || !environment.baseLibraryOk) {
      lines.push(`需要 Android≥${WEBGPU_MIN_ANDROID} 微信≥${WEBGPU_MIN_WECHAT} 基础库≥${WEBGPU_MIN_BASE_LIBRARY}`);
    }
  }
  return lines;
}

function paintReport() {
  const context = screen.getContext('2d');
  // A failed WebGPU attempt can leave the canvas bound; the modal still carries the result.
  if (!context) return;
  const ratio = windowInfo.pixelRatio;
  const line = 22 * ratio;
  context.fillStyle = '#142028';
  context.fillRect(0, 0, screen.width, screen.height);
  context.font = `${15 * ratio}px sans-serif`;
  let y = 60 * ratio;
  for (const text of ['UE 真机能力探针', ...summaryLines(), '', '点屏幕：复制报告 / 可选内存测试']) {
    context.fillStyle = /不可用/.test(text) ? '#ffc08a' : '#d6efe6';
    context.fillText(text, 18 * ratio, y);
    y += line;
  }
}

function copyReport() {
  wx.setClipboardData({ data: JSON.stringify(report, null, 2) });
}

async function runMemoryStress() {
  setStatus('内存测试中');
  try {
    const { instance } = await WXWebAssembly.instantiate('features/memory-probe.wasm', {});
    const { grow, touch } = instance.exports;
    let pages = 16;
    touch(0, pages);
    for (;;) {
      // Written before the attempt: the OS kills an over-budget game without any callback.
      wx.setStorageSync(MEMORY_KEY, { lastOkMB: pages / 16, attemptMB: (pages + MEMORY_STEP_PAGES) / 16, finished: false });
      if (pages + MEMORY_STEP_PAGES > 65536 || grow(MEMORY_STEP_PAGES) < 0) break;
      touch(pages, MEMORY_STEP_PAGES);
      pages += MEMORY_STEP_PAGES;
      setStatus(`已占用 ${pages / 16} MB`);
      await delay(60);
    }
    report.memory = { maxMB: pages / 16, ended: 'allocation refused or 4 GiB reached' };
    wx.setStorageSync(MEMORY_KEY, { lastOkMB: pages / 16, finished: true });
  } catch (error) {
    report.memory = { detail: errorText(error) };
  }
  wx.hideLoading();
  wx.showModal({ title: '内存测试完成', content: JSON.stringify(report.memory), showCancel: false });
  console.log('[UE-WASM-DEVICE]', JSON.stringify(report));
}

function readPreviousMemoryRun() {
  const previous = wx.getStorageSync(MEMORY_KEY);
  if (previous && previous.finished === false) {
    report.memory = {
      killedDuringPreviousRun: true,
      lastOkMB: previous.lastOkMB,
      killedAtAttemptMB: previous.attemptMB,
    };
    wx.removeStorageSync(MEMORY_KEY);
  } else if (previous && previous.finished) {
    report.memory = { maxMB: previous.lastOkMB, ended: 'previous run' };
  }
}

function showMenu() {
  if (!finished) return;
  wx.showActionSheet({
    itemList: ['复制完整报告', '内存上限测试（可能闪退）', '原生 memory64（可能闪退）'],
    success: async ({ tapIndex }) => {
      if (tapIndex === 0) return copyReport();
      if (tapIndex === 1) return runMemoryStress();
      finished = false;
      await checkWasmFeature('memory64');
      finished = true;
      wx.hideLoading();
      const result = report.wasm.memory64;
      console.log('[UE-WASM-DEVICE]', JSON.stringify(report));
      wx.showModal({
        title: 'memory64 测试完成',
        content: result.ok ? '通过' : result.detail,
        confirmText: '复制报告',
        success: ({ confirm }) => { if (confirm) copyReport(); },
      });
    },
  });
}

async function run() {
  setStatus('探针：设备信息');
  collectDevice();
  await collectBenchmark();
  collectHost();
  readPreviousMemoryRun();
  // Native memory64 can pause the mobile runtime and is optional for the lowered UE build.
  report.wasm.memory64 = { ok: false, skipped: true, detail: 'run manually from the probe menu' };
  if (typeof WXWebAssembly === 'undefined') {
    for (const name of WASM_FEATURES) report.wasm[name] = { ok: false, detail: 'WXWebAssembly missing' };
  } else {
    for (const name of WASM_FEATURES) await checkWasmFeature(name);
  }
  setStatus('探针：WebGPU');
  await checkWebGPU();
  // WeChat refuses a WebGPU context once WebGL has been used, so only one of the two is tried in a run.
  if (report.webgpu.source) {
    report.webgl2 = { ok: false, skipped: true, detail: 'not tested: WebGPU and WebGL cannot be used together' };
  } else {
    checkWebGL2();
  }
  if (report.webgpu.adapter) {
    setStatus('探针：Dawn');
    await runDawnTriangle();
    report.webgpu.device = Boolean(report.dawn.ok);
  } else {
    report.dawn = { ok: false, detail: 'skipped: no WebGPU adapter' };
  }
  // The triangle waits on adapter, device and GPU submission futures, which is the suspend path the engine needs.
  report.asyncify = report.dawn.ok
    ? { ok: true, detail: 'covered by the Dawn futures' }
    : { ok: false, detail: 'not proven: Dawn step did not finish' };
  buildVerdict();
  if (!report.dawn.ok) paintReport();
}

wx.onTouchEnd(showMenu);

run()
  .catch(error => { report.fatal = errorText(error); })
  .then(() => {
    finished = true;
    wx.hideLoading();
    console.log('[UE-WASM-DEVICE]', JSON.stringify(report));
    wx.showModal({
      title: '探测完成',
      content: report.fatal || summaryLines().join('\n'),
      confirmText: '复制报告',
      success: ({ confirm }) => { if (confirm) copyReport(); },
    });
  });
