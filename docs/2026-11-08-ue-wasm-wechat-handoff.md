# BoundlessImmortality 微信小游戏移植交接

2026-11-08 · 本机 Windows · 游戏仓库 `D:\demo\BoundlessImmortality`

## 目标和边界

沿用已选的 UE → WASM → 微信小游戏路线。目标是用独立的 UE 源码构建，将游戏作为微信小游戏运行，经过安卓和 iOS 真机验收后上传体验版、提审和发布。先跑 UE 空场景，再迁入真实游戏。所有实验在独立引擎目录和测试工程完成。保留主仓库中的已有资产、存档、未提交文件和当前 UE 5.8.4 安装。

## 当前证据

| 事项 | 状态 |
| --- | --- |
| 微信基础 WASM | 开发者工具 Stable `2.02.2608070`、小游戏基础库 `3.18.0` 的 **Windows 模拟器**返回 `answer() = 42`。报告的 `platform` 是 `devtools`。 |
| 微信模拟器 GPU | `webgl2`、`navigator.gpu.requestAdapter()` / `requestDevice()`、Canvas `webgpu` 上下文均通过。JSPI 的 `WebAssembly.Suspending` / `promising` 和浏览器 Canvas 转移接口缺失。 |
| Asyncify | 模拟器报告 `GPU copied 42 and resumed WASM`。这是真实 GPU 缓冲区回读的探针结果。 |
| Dawn C++ 三角形 | 用 UE 分支 vendored `emdawnwebgpu`、`MEMORY64=2` 和 `ASYNCIFY=1` 编译。Node 回归用真实 WASM、模拟 GPU，走通适配器、设备、GPU 提交三次 Future 等待。用户在 `document.querySelector` 修正后反馈「可以了」。交接时未收到修正后的三角形截图或 `[UE-WASM-DAWN]` JSON。 |
| UE 引擎与游戏 | 尚无编译完成的自定义编辑器、UE 空场景 HTML5 包或 BoundlessImmortality 小游戏包。暂无安卓或 iOS 真机、体验版、提审或发布证据。 |
| 平台账号 | 用户已能登录微信开发者工具，尚未提供有开发权限的正式小游戏 AppID。先用现有探针测真机。 |

## 本机文件和环境

- **游戏仓库** `D:\demo\BoundlessImmortality`。交接时 `main` 为 `a486e36`，原有修改和未跟踪文件需保留。根目录 `project.config.json` 和 `project.private.config.json` 是本次微信开发者工具调试产生的未跟踪文件。当前 `miniprogramRoot` 和 `srcMiniprogramRoot` 都指向 `Saved/Codex/UEWasmDawnSpike/`。正式小游戏工程应放独立目录。
- **引擎** `D:\demo\BoundlessImmortality\Saved\Codex\UEWasmEngine` 是已验证提交 `51fb9a63563a7f5c6d87aa470d3a3d5847cb7e72` 的稀疏检出，含 `Engine/Build` 和 `Engine/Platforms/HTML5`。完整编译副本为 `G:\CodexUEWasm\EngineSource`。交接时已复查 G 盘副本仍为同一稀疏检出，缺 `Core.Build.cs`、`GitDependencies.exe` 和自编译编辑器。此前本机凭据成功读取私有分支，随后完整检出遇到 GitHub 连接超时和连接重置。 [引擎源访问指南](https://github.com/PooyaEimandar/UnrealEngine-WebGPU/blob/main/docs/source-access.md) · [固定提交](https://github.com/PooyaEimandar/UnrealEngine/commit/51fb9a63563a7f5c6d87aa470d3a3d5847cb7e72)
- **基础宿主探针** `D:\demo\BoundlessImmortality\Saved\Codex\UEWasmSpike`，输出 `[UE-WASM-HOST]`。测试本地 WASM、JSPI、Canvas 和 Asyncify GPU 回读。
- **Dawn 探针** `D:\demo\BoundlessImmortality\Saved\Codex\UEWasmDawnSpike`，输出 `[UE-WASM-DAWN]`。成功应看到深蓝底、青色三角形，触屏复制报告。源码 `dawn-probe.cpp`、`dawn-probe-host.js` 和前一探针的 `probe.cpp`、`asyncify-probe.cpp` 在同级 `UEWasmTools`。`Saved/` 被 Git 忽略，Claude 若运行在另一台机器，需要单独给它这两个探针目录和上述源码，私有引擎由有权限的账号按提交拉取。
- **工具链** `D:\demo\BoundlessImmortality\Saved\Codex\UEWasmTools\emsdk` 已安装并验证 Emscripten `6.0.10`。VS 2022 `17.14` 位于 `D:\VS\IDE`，其中有 CMake `3.31.6`、Ninja `1.12.1`，机器有 Windows SDK。`UEWasmTools\rustup` 和 `cargo` 中的 Rust 安装不完整，直接执行报缺 toolchain manifest，重装或修复后用 `rustc --version` 和 `cargo --version` 验证。Rust 和 Emscripten 环境变量按构建会话设置。
- **开发者工具** `D:\demo\BoundlessImmortality\Saved\Codex\UEWasmTools\WeixinDevTools\微信开发者工具.exe`。CLI 服务端口未启用，可以手动打开项目、编译和预览。此前 `/__dev__/WAGame.js` 500 来自工具缓存 `EXDEV`。官方基础库 3.18.0 补入缓存后，日志记录加载成功。复发时先查 `[vendor-timeline]`，按实际缺失的版本核对缓存。

## 复编 Dawn 探针

在 `D:\demo\BoundlessImmortality` 打开 PowerShell。输出可用微信开发者工具直接导入。

```powershell
$toolsDir = (Resolve-Path 'Saved/Codex/UEWasmTools').Path
$sdk = Join-Path $toolsDir 'emsdk'
$env:EMSDK = $sdk
$env:EM_CONFIG = Join-Path $sdk '.emscripten'
$env:EMSDK_PYTHON = Join-Path $sdk 'python/3.13.3_64bit/python.exe'
$env:EMSDK_NODE = Join-Path $sdk 'node/24.19.0_64bit/node.exe'
$port = 'Saved/Codex/UEWasmEngine/Engine/Platforms/HTML5/Source/ThirdParty/Dawn/emdawnwebgpu_pkg/emdawnwebgpu.port.py'
& "$sdk/upstream/emscripten/em++.exe" -O2 -sMEMORY64=2 -sASYNCIFY=1 `
  -sASYNCIFY_STACK_SIZE=16384 -sMODULARIZE=1 -sEXPORT_NAME=createDawnProbe `
  -sENVIRONMENT=web '-sEXPORTED_RUNTIME_METHODS=ccall' --no-entry `
  "--use-port=$port" --pre-js "$toolsDir/dawn-probe-host.js" `
  "$toolsDir/dawn-probe.cpp" -o 'Saved/Codex/UEWasmDawnSpike/dawn-probe.js'
if ($LASTEXITCODE -ne 0) { throw 'Dawn probe build failed' }
```

在微信环境，`globalThis.document` 可能存在但没有 `querySelector`。`dawn-probe-host.js` 在 Emscripten 模块内将 `#canvas` 映射到 `Module.canvas`，避免修改全局对象。小游戏入口通过 `WXWebAssembly.instantiate(包内路径, imports)` 接管模块加载。对照组应包含 `document={}`，这一环境可复现旧报错。

## 接手顺序和验收

1. **尽早取得真机权限。** 让用户提供一个由其控制或有开发权限的微信小游戏 AppID，并在开发者工具里登录和授权。先把两个现成探针跑在安卓和 iOS 上，记录设备、微信版本、基础库、完整 JSON 和三角形画面。两端的 WASM、Dawn、WebGPU 实际证据决定可支持的设备范围。若目标设备缺 WebGPU，先明确支持范围或另评估渲染后端，模拟器的 WebGL2 结果无法代替这一步。 [微信公众平台](https://mp.weixin.qq.com/) · [小游戏预览与体验版流程](https://docs.unity.cn/cn/tuanjiemanual/Manual/UploadWeixinMiniGame.html)
2. **恢复 G 盘完整引擎检出并建立浏览器基线。** 用 `git -C G:\CodexUEWasm\EngineSource ls-remote --exit-code origin refs/heads/5.8-webgpu` 验证网络与权限。确认 HEAD 仍为固定提交后，运行 `git -C G:\CodexUEWasm\EngineSource sparse-checkout disable`。G 盘交接时约有 205 GiB 空闲。先检查依赖下载与编译空间，再按分支说明配置 VS x64、Rust、Emscripten、Dawn、Tint 和 HTML5 第三方库。以 `RunUBT.bat -Mode=ValidatePlatforms -Platforms=Win64+HTML5`、编辑器/打包工具构建日志和空项目的浏览器画面作为证据。该分支文档仍缺 Windows 端到端构建记录，若卡在 Windows 工具链，可使用已有验证记录的 Linux 路线。 [Windows 构建步骤](https://github.com/PooyaEimandar/UnrealEngine-WebGPU/blob/main/docs/build-windows.md) · [Linux 构建步骤](https://github.com/PooyaEimandar/UnrealEngine-WebGPU/blob/main/docs/build-linux.md)
3. **把独立 UE 空场景迁入小游戏。** 先使用 `wasm64nothreads` 和默认 lowered `MEMORY64=2`，避开浏览器 Worker 共享内存与 Canvas 转移。在引擎分支实现可选的 Asyncify 链接路径，保留原有浏览器 JSPI 路径。起点是 `Engine/Platforms/HTML5/Source/Programs/UnrealBuildTool/HTML5ToolChain.cs` 中的 `-sJSPI=1` 和 `ASYNCIFY_IGNORE_INDIRECT` 设置。后者的 JSPI 假设需要重新验证。首次先量化 Asyncify 的栈和包体成本，验证 GPU 等待、帧循环与 I/O 挂起后再精简插桩。
4. **适配微信宿主边界。** 以小游戏 `game.js` 和实际 WX Canvas 替换 HTML 页面引导，复用探针中已验证的实例化和画布绑定。读 `Build/Templates/index.html.template`、`UEDeployHTML5.cs`、`Source/Runtime/Launch/Private/HTML5/LaunchHTML5.cpp` 和分支 `docs/ENGINE_TOUCHPOINTS.md`。按启动顺序验证资源挂载与 IO，再接触控、音频、暂停/恢复、存档和 HTTPS。检查首包、WASM 与资源分包、冷启时长、峰值内存、长时间运行和断网。验收以 UE 空场景在实际小游戏宿主启动、渲染和响应触控为准。
5. **迁真实游戏的一条闭环。** 主工程为 UE 5.8.4，分支基线为 5.8.3。先在副本中验证资产版本兼容性，保存修改仅落在移植副本。先迁一张小图、一个角色、一种怪和一次战斗结算，逐步接回 Slate/UMG、Niagara 与存档。`NiagaraUIRenderer` 当前限 Win64/Android，游戏 `Build.cs` 直接依赖它。`DouyinLiveProvider` 为 Win64 模块，首轮小游戏目标需隔离平台绑定。武灵 `Tools/CultivationService` 是本机原型服务，生产账号与部署单独实施。
6. **上线。** 由账号主体按微信小游戏后台当前要求核对类目和资质、域名、隐私、登录及需要的商业功能。接入后对 iOS、安卓体验版做完整闭环与性能验收。保留构建版本、测试记录、提交审核和审核结果。具备发布权限后再正式发布，并留存线上版本号与扫码或入口复核记录。

## 可直接发给 Claude 的任务

> 请读取 `D:\demo\BoundlessImmortality\docs\2026-11-08-ue-wasm-wechat-handoff.md`，接手 UE→WASM→微信小游戏移植。先复核 G 盘引擎检出和 Rust 状态，按固定提交建立 UE 空场景浏览器基线。并行推进小游戏 AppID 和两端真机探针。随后在独立引擎与项目副本中实现单线程 Asyncify 和微信宿主适配，做到空场景、真实玩法、体验版、提审、发布逐阶段验收。保护现有游戏资产、存档和未提交工作。每阶段明确区分离线、开发者工具模拟器、真机和线上证据。需要账号本人扫码、资质或发布权限时，把具体操作交给我。
