# 无极仙道 · Slate 修仙 UI 系统（本地验证版）

日期：2026-10-28。范围：重构 UI 系统与 UI 表现，竖屏优先、横屏重排；**不接入直播 SDK**，所有依赖联网服务的界面使用明确标注的本地验证数据。

## 为什么重做

旧前端是一个约 1.2 万行的 UMG 命令式构建体（`UInkFrontendWidget` + 各类 FeaturePanel），40 余个功能页共用同一种扁平宣纸表单：标题被裁切、字号偏小、按钮为平涂色块、缺少层级与动效。新系统改为纯 Slate 自绘，直接复用项目里已有的高质量修仙美术（竖版仙鹤山水卷轴、云纹金按钮、八角金框匾额、道具与属性图标、场景横幅、宝箱），由程序叠加光晕、扫光、描金、粒子与转场动效。

## 目录与分层

`Source/BoundlessImmortality/InkUI/`

| 文件 | 职责 |
|---|---|
| `InkUITokens` | 设计令牌：玄漆/鎏金/玉青/朱砂/宣纸色板、品质色、字体角色（马善政标题 + 霞鹜文楷正文与数字）、间距与动效时长 |
| `InkUIArt` | 纹理与字体注册表（GC 安全）、多段切片皮肤表、道具/属性/场景/境界牌匾图集格位 |
| `InkUIMotion` | 缓动曲线、可中断补间 `FTween`、错峰、呼吸脉冲、减少动效开关 |
| `InkUIDraw` | `FPainter`：多段切片皮肤、渐变、径向辉光、光晕、光芒、扫光、描金框、回纹角花、祥云、分隔线、红点、灵气粒子 |
| `InkUIModel` | 界面模型：玩家、属性、背包、货币、排行、签到、活动、仙途、存档、设置；分区修订号避免实时刷新导致整页重建 |
| `InkUIWidgets` + `InkUIControls/Elements/Windows` | 组件库：动画盒、按钮（六态）、HUD 图标钮、关闭钮、进度条（缓冲拖尾）、道具格、头像、滑动页签、卷轴窗口、提示、确认框、奖励演出 |
| `InkUIRoot` | 根节点：基础层（主菜单/HUD）→ 遮罩 → 页面栈 → 弹层 → 提示；Esc/返回逐层关闭；横竖屏迟滞判定；退出动画完成后再移除 |
| `InkUIScreen*` | 主菜单、HUD、角色、背包、排行、签到、活动、仙途、设置、存档 |
| `InkGameUIWidget` | UMG 宿主：挂入视口、把界面命令转给 `AInkMenuDirector` 与玩法组件、拉取实时身份/血量/修为、暴露测试钩子 |

## 数据边界

- **实时（本地游戏）**：主播名、境界、血量、修为、修炼速度、入定状态（`UCultivationSubsystem` / `UCultivationGameplayComponent`）；属性与本局排行由 `PublishGameplayData` 推送；存档读写走 `UInkSaveSubsystem`；画质与帧率走 `UGameUserSettings`。
- **本地验证数据**：背包物品、灵石/仙玉、签到、活动、仙途入口、无实时数据时的排行。界面上以“本地”“本地验证数据 · 非全服榜”“需联网”等字样标明，交易类操作提示“服务未接入”，不伪造到账或全服结果。
- SDK 接入时只需替换模型的填充来源（`UInkGameUIWidget` 的拉取与命令处理），界面层不变。

## 动效约定

页面打开：遮罩 0.22s 淡入，卷轴窗口 0.34s 回弹缩放 + 下落，同时两点金光沿金框“描金”0.95s；内容按 0.03s 错峰上浮。按钮按下 0.07s 缩至 0.97，松开 0.12s 回弹；主按钮呼吸光晕 + 3.4s 周期扫光。页签选中匾额 0.18s 滑动。进度条数值 0.32s 缓动，掉血留白色拖尾。奖励演出：旋转光芒、标题回弹、道具依次弹出、仙品以上冲击环。HUD：身份卡左滑入、顶部入口依次落下、底栏上浮、中心修炼呼吸光环；入定时光芒旋转、金色粒子上升、“+修为”飘字。减少动效模式关闭位移、循环与粒子，只保留短淡入。

## 运行与验证

- 游戏只有这一套 UI。旧 UMG 前端（`UInkFrontendWidget` 及其 40 余个功能面板、`-LegacyInkUI` 开关、旧 PIE 脚本、`Tools/WulingQA`、包体验收 `-WulingPackageAcceptance`）已于 2026-11-01 移除，见下文“UI 资源清理”。
- 本地验证脚本：`Tools/InkFrontend/verify_ink_slate_ui_pie.py`，在 720×1280 竖屏 PIE 中点击各入口并截图（含动画中途帧），再切 1280×720 横屏复核。截图与 `report.json` 输出到 `Saved/InkUI/Verification/<时间>/`。
- 点击方式（`UInkGameUIWidget::ClickTestTarget`）：取目标控件的 tick 空间几何（桌面坐标）中心，在游戏窗口自己的 Slate 命中网格里求冒泡路径，路径不含目标即判失败（能抓出被其他图层遮挡的按钮），再沿该路径投递移动/按下/抬起指针事件。不移动系统光标，因此 PIE 窗口被编辑器或其他程序遮住时也能验证；绘制空间几何在窗口绘制期间是窗口局部坐标，不能用于命中。

```powershell
UnrealEditor.exe BoundlessImmortality.uproject -unattended -NoLiveCoding -NoAutoSave -nosplash -ExecutePythonScript=Tools/InkFrontend/verify_ink_slate_ui_pie.py
```

`-unattended` 用于屏蔽模态对话框（例如本地 Zen 缓存服务启动较慢时的“Wait for ZenServer?”）；脚本内部调用 `set_keep_python_script_alive(True)`，否则引擎会在脚本返回后的下一帧关闭编辑器。

验证边界：仅 Windows 编辑器 PIE；未做安卓/iOS 真机、刘海安全区实机、性能采样与直播推流验证。

## UI 资源清理（2026-11-01）

用户要求“清理 UI 的项目结构，删除没有用到的 UI 资源”。做法：扫描源码与配置里的 `/Game/UI/...` 路径（经 `InkUiAssets::Resolve` 的旧路径映射展开），加上资产注册表里 UI 以外资产（地图、蓝图、特效）对 UI 资产的引用，求依赖闭包；闭包外的全部删除。

- `Content/UI` 从 587 个资产（208 MB）减到 209 个（174 MB）：新 Slate UI、HUD 回退层、GM 面板、头顶称号与名牌实际加载的纹理、材质、Niagara、字体与主题数据。删除的 377 个是旧前端的 14 个 WBP、它们的 300 多个材质实例与材质、旧修订目录（`Cohesion20261021`、`NiagaraRevision20261022`、`Refactor20261021`、`Provided` 等）留下的重定向器。保留 `Materials/Shared/M_InkTitleAura`（称号光环特效的构建源材质）。
- 主地图 `Level_Sky_Temple_Demo_01` 重存一次，去掉菜单导演对 `WBP_InkFrontend` 的类引用。
- `DefaultEngine.ini` 删去 363 条指向已删资产的 `PackageRedirects`，`InkUiAssetPaths.cpp` 的旧路径映射同步裁到 206 条。
- 代码：删除旧前端源文件 56 个及其测试；`AInkMenuDirector` 只创建 `UInkGameUIWidget`；HUD 回退层、GM 面板、称号特效原先查询旧前端“是否在 HUD 页/是否减少动效”，改为查询新 UI（`IsHudVisible`、`GetTopRoute`、`IsReducedMotion`）。编辑器库 `UInkFrontendEditorLibrary` 只保留验证脚本使用的 PIE 启动、调尺寸、截图与 GM 按钮点击。
- 所有被删文件都在 git 的首个提交里，可随时从历史恢复。

## 物品系统页面（2026-11-01）

新增页面：灵玉商城（`JadeShop`）、武灵商城（`WulingShop`）、仙道殿（`XianDao`）、每日一签（`Fortune`）、境界突破（`Breakthrough`）、物品来源（`ItemSources`）、武灵建设（`GuildConstruction`）、装备法宝（`EquipmentFabao`）。HUD 右侧第二排为“每日一签”“仙道殿”；背包头部改为灵石与四货币五枚钱包签；角色页“突破”在游戏中打开境界突破页。共用的纸卡、匾牌、按钮、钱包条在 `InkUI/InkUIXianTuKit.h`。玩法与数据见 `docs/2026-11-01-xiantu-item-economy.md`，验证脚本 `Tools/InkFrontend/verify_ink_economy_pie.py`。

## 构建说明

游戏模块改为 `bUseUnity = false`：提交全部源码后，多个翻译单元中的同名文件内辅助函数（`Number`、`Obj`、`Flag` 等）在 Unity 合并编译下互相冲突，导致干净检出无法编译。
