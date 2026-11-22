# UE → 微信小游戏移植状态

对应 `docs/2026-11-08-ue-wasm-wechat-handoff.md`。证据按 **离线 / 开发者工具模拟器 / 真机 / 线上** 分开记录，只有真机及以上能证明手机可用。

最后更新：2026-11-08

## 证据

| 项目 | 离线 | 模拟器 | 真机 | 线上 |
| --- | --- | --- | --- | --- |
| WASM 特性（SIMD、批量内存、BigInt 等） | 通过 | 通过 | **通过**（nubia NX789J）。原生 memory64 在微信 8.0.69 上不支持，升到 8.0.79 后支持；UE 用的是降级模式，两种情况都不受影响 | — |
| WebGL2 | — | 通过 | **通过**（Adreno 830，支持 ASTC、ETC） | — |
| WebGPU | 探针逻辑回归通过 | 通过，配额达标 | **未验证**：这台手机已升到微信 8.0.79（达标），但基础库还是 3.15.3，低于要求的 3.17.3，`wx.getGPU` 不存在 | — |
| Dawn C++ 三角形、Asyncify 等待 GPU | 胶水代码回归通过 | 通过 | 未验证（同上） | — |
| Asyncify 链接路径（引擎分支） | UnrealBuildTool 编译通过（0 错误），还没有实际链接过引擎 | — | — | — |
| 平台校验 `ValidatePlatforms` | `Win64 VALID`、`HTML5 VALID` | — | — | — |
| UE 空场景 | 未开始 | — | — | — |
| 真实游戏闭环 | 未开始 | — | — | — |

## 环境

| 项目 | 状态 |
| --- | --- |
| Rust | 已修复，`rustc 1.99.1`、`cargo 1.99.1`（`Saved/Codex/UEWasmTools`） |
| 引擎检出 | **已完成**：固定提交 `51fb9a63`，190500 个对象全部到位，工作区已展开。脚本 `Tools/WeChatPort/Sync-EngineSource.ps1` |
| 引擎依赖 | **已完成**：128950 个文件、23.3 GiB，排除了 Mac、iOS、tvOS、Android、Linux。G 盘剩余约 121 GB |
| 引擎构建 | deps、validate 已完成，后续阶段未开始。分阶段脚本 `Tools/WeChatPort/Build-EngineBaseline.ps1`，阶段：deps、validate、dawn、tint、thirdparty、editor、tools、html5。日志 `G:\CodexUEWasm\logs\baseline-*.log` |
| 网络 | GitHub 经代理和直连都时通时断，分批下载能扛住 |

引擎改动只在 `G:\CodexUEWasm\EngineSource` 的工作区，未提交：

- `Source/Programs/UnrealBuildTool/HTML5ToolChain.cs`
- `Source/Developer/HTML5TargetPlatformSettings/Classes/HTML5TargetSettings.h`
- `Source/Developer/HTML5TargetPlatformSettings/Private/HTML5TargetPlatformClasses.cpp`

新增工程设置 `bUseAsyncify`、`AsyncifyStackSizeKB`、`bAsyncifyIgnoreIndirect`，默认全部关闭，浏览器 JSPI 路径不变。

## 路线判断

### 1. 官方 WebGPU：目前只有 Android

[官方 WebGPU 指南](https://developers.weixin.qq.com/minigame/dev/guide/performance/WebGPU.html)（2026-10-29 更新）：通过 `wx.getGPU()` 获取，要求 Android ≥ 10、微信 ≥ 8.0.79、基础库 ≥ 3.17.3。iOS 和鸿蒙“正在建设中”，没有给日期。

`Canvas.getContext` 的 API 页面没有列出 `webgpu`，只看那一页会误判，指南和基础库更新日志（v3.17.3）才是准确来源。

### 2. 官方限制对 UE 分支的影响

| 官方限制 | 分支现状 | 需要的处理 |
| --- | --- | --- |
| 不支持 `createRenderPipelineAsync`、`createComputePipelineAsync` | `WebGPUDevice.cpp:254`、`:279` 正是这样创建管线的 | 微信构建改用同步创建 |
| device 须在 `context.configure` 之后使用 | Surface 在 `WebGPUViewport.cpp:156` 首次出帧时才配置，此前已创建资源 | 调整初始化顺序 |
| 不支持多渲染目标，只有 `colorAttachments[0]` 生效 | 命令上下文按引擎目标数提交；移动端深度预通道可能带速度目标 | 关闭用到 MRT 的渲染特性 |
| 只能有一个 device，`requiredFeatures`、`requiredLimits` 不生效 | RHI 按适配器上限请求 | 启动检查改读 `adapter.limits` |
| WebGPU 和 WebGL 不能同时使用 | 引擎只用 WebGPU | 宿主层不要创建 WebGL 上下文 |
| `popErrorScope` 恒返回 null | RHI 用错误作用域记录日志 | 调试改用 `uncapturederror` |

官方另有 [wxWebGPU C++ SDK](https://git.weixin.qq.com/wechat-minigame/webgpu_cpp_sdk)，文档说在 WASM 内直接编码渲染指令，性能好于 JS 调用。它可能替换 `emdawnwebgpu`，但仓库要微信登录才能查看，还没有评估。

### 3. 包体上限

[代码包](https://developers.weixin.qq.com/minigame/dev/guide/base-ability/code-package.html)总计不超过 30 MB，主包不超过 4 MB；[`WXWebAssembly.instantiate`](https://developers.weixin.qq.com/minigame/dev/guide/performance/perf-webassembly.html) 只接受包内路径（支持 `.wasm.br`）。分支自己的实测（`docs/M10-SHIPPING.md`）里 Shipping WASM 为 127.37 MiB、gzip 后 36.72 MiB，这还是没有 Asyncify 插桩的体积。要实测才能知道能否进 30 MB。

## 手机的基础库怎么更新

基础库和微信客户端分开更新。[官方说明](https://developers.weixin.qq.com/minigame/dev/guide/runtime/client-lib/)：客户端发布时带的是上一个稳定版基础库，新版本由后台灰度下发，一般要 12～24 小时；每个基础库只能在对应的客户端版本上运行。

不想等灰度时，开发者工具可以把基础库推送给自己：**详情 → 本地设置 → 调试基础库**，在下拉框选版本，点旁边的「推送」并确认。工具的提示写明只推送到当前登录用户的设备，不影响其他用户。

- 入口和行为来自开发者工具的代码（提示文案和 `wxa-dev-logic/push_commlib` 请求）。查过的官方页面没有写这个流程，手机上还没有实际用过。
- 选 3.17.x 的最高版本（更新日志目前到 3.17.6）。3.18.0 还没有出现在更新日志里，微信 8.0.79 不一定能运行。

## 下一步的分支

| 基础库更新后的真机结果 | 下一步 |
| --- | --- |
| Dawn 三角形显示，`canRunUEWebGPUFork` 为真 | 继续交接第 2～6 步，同时处理上表的限制 |
| 有 `wx.getGPU`，但 Dawn 失败 | 按报告里的失败阶段修宿主或胶水 |
| 推送基础库失败，或推送后 `SDKVersion` 仍低于 3.17.3 | 等后台灰度，或换一台满足要求的 Android 测试 |

只有 Android 可用这一点需要产品决定：是否接受首发只覆盖 Android，等官方 iOS 支持后再扩展。如果一定要同时覆盖 iOS，备选是给 UE 5.8 补 WebGL2 后端（引擎级大工程）、UE 4.27 ES3 路线（资产和 C++ 要降级重做）、或按策划案的止损路线用 Godot/Cocos 重做轻量客户端。
