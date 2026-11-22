# 微信小游戏工程

UE → WASM → 微信小游戏移植的独立目录。主工程的资产、存档和 `Saved/` 下的实验都不在这里。

| 目录 | 内容 |
| --- | --- |
| `device-probe/` | 真机能力探针，可直接用微信开发者工具导入 |
| `device-probe-src/` | 探针的 C++/JS 源码、特性模块生成脚本和离线测试 |

## 跑真机探针

1. 微信开发者工具 → 小游戏 → 导入 `MiniGame/device-probe`。`project.config.json` 里的 AppID 取自根目录调试配置，如果工具提示无权限，换成你有开发权限的小游戏 AppID。
2. 点工具栏「预览」，分别用安卓和 iPhone 的微信扫码。两台手机上的微信都要是已更新的版本。
3. 等「探测完成」弹窗，点「复制报告」，把 JSON 发回来。没弹窗时点一下屏幕，再选「复制完整报告」。
4. 想测内存上限时，点屏幕选「内存上限测试」。这项可能让小游戏闪退；再次打开时报告里会记下闪退前的内存数值。

开发者工具模拟器基于 Chromium，模拟器通过不能代表手机能运行，判断路线只看手机上的报告。

手机要满足[官方 WebGPU 要求](https://developers.weixin.qq.com/minigame/dev/guide/performance/WebGPU.html)：Android ≥ 10、微信 ≥ 8.0.79、基础库 ≥ 3.17.3。iOS 和鸿蒙官方还在建设中。版本不够时，报告的弹窗会直接写明。

微信版本够了但基础库不够时，在开发者工具的「详情 → 本地设置 → 调试基础库」里选 3.17.x 的最高版本，点旁边的「推送」。手机上完全退出微信再打开，然后重新预览。推送只影响开发者工具登录的微信账号，手机要登录同一个账号。

## 探针怎么判定

报告的 `verdict` 字段：

| 字段 | 含义 |
| --- | --- |
| `wasmFeaturesForUE` | UE 构建用到的 SIMD、批量内存、符号扩展、饱和转换、BigInt 互操作都可用 |
| `webgl2` | 能创建 WebGL2 上下文。有 WebGPU 时不测，官方规定两者不能同时使用，此时值为 `not tested` |
| `webgpuDevice` | Dawn 成功创建了 WebGPU 设备并完成绘制。官方只允许一个设备，所以只在 Dawn 这一步创建 |
| `webgpuBudgetForUE` | 存储缓冲区 ≥10、存储纹理 ≥8，达到 WebGPURHI 的启动要求 |
| `asyncifyOverGpu` | Asyncify 能等待真实 GPU 的 Future，由 Dawn 三角形这一步证明 |
| `dawnTriangle` | Dawn 的 C++ 三角形能画到屏幕（应看到深蓝底、青色三角形） |
| `canRunUEWebGPUFork` | 以上条件全部满足，当前 UE WebGPU 分支才有在这台手机上跑的前提 |

## 离线回归

```bash
python MiniGame/device-probe-src/generate_feature_modules.py MiniGame/device-probe/features
```

```bash
node MiniGame/device-probe-src/check_features.js MiniGame/device-probe/features
```

```bash
node MiniGame/device-probe-src/test_probe.js MiniGame/device-probe
```

```bash
node MiniGame/device-probe-src/check_glue.js MiniGame/device-probe
```

这些只验证探针自身逻辑，属于离线证据。

## 上传前的语法要求

预览和上传时，微信服务端会校验包内 JS，ES2020 语法（`?.`、`??`、BigInt 字面量、类字段等）会报 `invalid file`。Emscripten 输出的胶水代码要先降级：

```bash
node MiniGame/tools/lower-glue.mjs MiniGame/device-probe/dawn-probe.js
```

脚本按语法树改写，改写后必须能按 ES2017 解析，否则直接报错。重新编译探针后要再跑一次。项目配置同时打开 `es6` 和 `enhance`，把 `async` 等语法继续转换下去。
