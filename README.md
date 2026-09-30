# Sokoban｜推箱子玩法与关卡制作工具

本项目是一份技术策划笔试作品，围绕“推箱子游戏机制、完整关卡体验流程、关卡编辑器，并以产品级要求制作”的题目展开。工程使用 Unreal Engine 5.6、C++、Enhanced Input 和 Slate，实现了可试玩的单关玩法，以及策划通过界面配置、校验、保存关卡数据的制作流程。

当前版本的 C++ 逻辑支持“制作关卡 → 配置关卡目录 → 主菜单 → 选关 → 推箱子 → 正式结算 → 保存成绩 → 下一关／重玩”的闭环；运行界面需在 Widget Blueprint 中设计并接线，参见 [UMG 蓝图接入](Docs/BlueprintUI.md)。所有关卡始终可选，不做关卡解锁；存档只记录成绩和最近关卡，不恢复中途棋盘。

## 1. 评审入口

| 希望了解的内容 | 建议入口 |
|---|---|
| 直接试玩与环境配置 | [工程配置与试玩接入](Source/Sokoban/Sokoban/EditorSetup.md) |
| 主菜单、选关、存档与结算配置 | [正式流程接入](Docs/Frontend.md) |
| 使用工具制作关卡 | [关卡制作与交付操作规范](Docs/LevelEditor.md) |
| 后续 AI Coding 的接口与边界 | [AI 开发约定](Docs/AI_DEVELOPMENT.md) |
| 数据结构与职责划分 | [核心代码说明](Source/Sokoban/Sokoban/README.md)、[SokobanTypes.h](Source/Sokoban/Sokoban/Data/SokobanTypes.h) |
| 移动、推动和撤销的判定 | [SokobanRules.cpp](Source/Sokoban/Sokoban/Rules/SokobanRules.cpp) |
| 单局状态、历史与通知 | [SokobanSession.cpp](Source/Sokoban/Sokoban/Gameplay/SokobanSession.cpp) |
| Slate 工具与资产保存 | [SSokobanLevelEditor.cpp](Source/SokobanEditor/Private/SSokobanLevelEditor.cpp)、[编辑文档模型](Source/SokobanEditor/Private/SokobanEditorDocument.cpp) |

建议先运行内置双箱示例，验证移动、撤销和通关，再使用关卡编辑器制作一个关卡并接入试玩，最后结合源码查看数据流与异常处理。

## 2. 已实现内容与设计考虑

| 模块 | 当前行为 | 对玩法与制作流程的作用 |
|---|---|---|
| 关卡数据 | 地形、目标点、玩家出生点、带稳定 ID 的箱子；使用 `USokobanLevelData` 保存 | 关卡设计与场景显示分开，一张试玩地图可以承载不同谜题 |
| 关卡校验 | 检查尺寸、数组、坐标、占用冲突、箱子 ID、箱子与目标数量 | 编辑阶段定位数据问题，运行时拒绝非法初始状态 |
| 推箱规则 | 四方向逐格移动、单箱推动、地形／边界／箱子阻挡、全箱到位判定 | 同一套规则服务玩家操作与历史操作，不依赖物理碰撞结果 |
| 单局管理 | 步数、推动次数、撤销、重做、重开、成功操作通知 | 规则失败不写状态；撤销恢复位置和计数，分支操作清理重做历史 |
| 输入与表现 | Enhanced Input、俯视镜头、网格到世界坐标转换、移动动画、颜色区分 | 输入转换为方向请求，表现层根据已经确认的结果播放动画 |
| 关卡编辑器 | Slate 三栏界面、单格配置、尺寸调整、编辑历史、问题定位、保存 DA | 策划通过“选中格子 → 配置内容 → 应用”制作关卡，无需手填地形数组 |
| 编辑保护 | 草稿与原资产隔离、未保存提醒、唯一 ID 检查、外部修改提示、保存失败保留草稿 | 降低制作中误覆盖与误丢失数据的风险 |
| 正式流程 | 主菜单、全部开放的选关、UMG HUD、单次正式结算与下一关 | 流程独立于单局规则，最后一步动画结束后结算 |
| 进度记录 | 按关卡 ID 与修订号保存最佳成绩、最近关卡 | 失败保留内存；不兼容存档写保护，确认后备份恢复 |
| 自动化验证 | 数据校验、规则、会话、玩法接入、编辑器与正式流程测试 | 验证关键边界及模块之间的约定 |

这里的“产品级要求”体现为职责分离、异常反馈、制作流程和可验证性。当前棋盘美术仍是灰盒阶段，运行界面的布局、样式和动画完全由 Widget Blueprint 定义，C++ 只提供数据、命令与页面管理，结构合法也不等于关卡一定可解。

## 3. 环境与运行

已验证开发环境：Windows 11、UE 5.6.1、Visual Studio 2022（MSVC 14.38、Windows SDK 10.0.22621）。工程关联版本为 UE 5.6。Rider 可用于阅读与开发，但 C++ 编译仍需相应工具链。

1. 获取完整的 `Source`、`Content`、`Config` 和 `Sokoban.uproject`，安装 UE 5.6 及 C++ 工具链。
2. 为 `Sokoban.uproject` 生成工程文件，编译 `SokobanEditor / Development Editor / Win64`，或在首次打开工程时同意重新编译模块。
3. 打开 `/Game/Sokoban/L_SokobanPlayerGround`，确认使用 `BP_SokobanGameMode`，点击 Play。
4. 按 [UMG 蓝图接入](Docs/BlueprintUI.md) 完成四个 WBP 的布局、事件和 UIManager 配置，再通过主菜单选择关卡。C++ 不再生成默认界面；缺少配置时查 Output Log。GameMode 的 `Level Catalog` 已接入两关示例。需要旧单关调试时，先关闭 `Enable Frontend`，再使用 `Startup Level` 或内置示例。

也可以在 PowerShell 中编译。以下两项路径需替换为本机安装和工程位置：

```powershell
$EngineRoot = 'D:\UE_5.6'
$ProjectRoot = 'G:\Sokoban'
& "$EngineRoot\Engine\Build\BatchFiles\Build.bat" SokobanEditor Win64 Development "-Project=$ProjectRoot\Sokoban.uproject" -WaitMutex
```

| 操作 | 按键 | 说明 |
|---|---|---|
| 移动／推动 | WASD 或方向键 | 每次按下或改变方向尝试一步；保持同一方向不会连续移动 |
| 撤销 | Z | 撤销最近一次成功操作，恢复计数 |
| 重做 | Y | 恢复最近撤销的操作 |
| 重开 | R | 恢复本关初始状态，清空历史 |

动画期间的新操作会被忽略，当前没有输入缓冲。内置示例可依次输入 **W、S、D、D、W** 通关，共 5 步、2 次推动；每次输入等待动画结束，两个 D 分别按下。

## 4. 关卡制作与试玩

UE 顶部选择 **窗口 → 推箱子关卡编辑器**。界面左侧配置关卡信息与尺寸，中间选择格子，右侧配置地形、目标点和占用物。

制作完成后，执行“校验关卡”和“保存资产”，将 DA 加入 `DA_SokobanCatalog` 的 Levels 数组，保存目录后重新 Play。工具保存允许保留未完成草稿，正式试玩前必须处理校验错误并人工验证解法。详见[关卡制作规范](Docs/LevelEditor.md)。

运行时棋盘依据数据动态生成。墙、地板和目标使用实例化网格，玩家与箱子使用独立的网格组件。无需在地图中逐个摆放箱子 Actor；工具中的格子配置就是当前关卡布局的制作入口。

## 5. 架构与目录

```text
Source/
  Sokoban/                  运行时模块
    Sokoban/
      Data/                 关卡资产、静态布局与运行时数据结构
      Rules/                校验、移动规划、应用与还原
      Gameplay/             Session、GameMode、Enhanced Input 控制器
      Presentation/         棋盘、相机、旧单关调试 HUD
      Flow/                 菜单、选关、游戏、结算状态协调
      Progress/             最佳成绩、存档读写与恢复
      UI/                   UI 数据与命令接口、控制器页面管理
  SokobanEditor/            仅在 UE 编辑器中加载的 Slate 工具模块
Content/Sokoban/            蓝图、输入资产、地图、关卡数据、默认材质
Config/                     默认地图、输入、模块迁移兼容等配置
Docs/                       关卡制作工作文档
Scripts/Editor/             默认材质创建辅助脚本
```

玩法数据流为 `Enhanced Input → PlayerController → GameMode → Session → Rules`；成功后由 `Session` 通知 `GameMode`，再更新棋盘表现。工具数据流为 `Slate 控件 → 编辑草稿 → USokobanLevelData → .uasset`。运行时复制 DA 中的布局初始化对局，游戏中的移动不会改写设计资产。

工程已从官方 TopDown 模板迁移到独立的 `Sokoban` 模块，自有玩法不依赖模板角色或模板控制器。规则层仍使用 UE 的类型与容器；迁移到其他 UE 工程需要同步模块配置、资源和输入接入，不是直接复制到任意标准 C++ 项目。

## 6. 验证与验收边界

2026-09-29 的本地验证记录：Editor 目标构建成功；Game 目标构建检查成功；自动化测试 42 项通过、0 失败、0 警告。测试分布如下：

| 测试组 | 数量 | 重点 |
|---|---:|---|
| `Sokoban.Validation` | 7 | 非法布局、坐标、占用与数量约束 |
| `Sokoban.Rules` | 10 | 移动／推动、记录校验、撤销还原、计数边界 |
| `Sokoban.Session` | 9 | 初始化、历史分支、通关状态、通知重入保护 |
| `Sokoban.Gameplay` | 4 | 输入配置、动画期间的操作限制、棋盘表现与完成时机 |
| `Sokoban.Editor` | 6 | 格子配置、尺寸变化、编辑历史、资产副本隔离、异常草稿、面板构建 |
| `Sokoban.Frontend` | 6 | 目录边界、成绩与修订、真实存档读写、单次结算、WBP 数据接口与布局归属、默认地图 PIE 接入 |

在 UE 自动化测试窗口中筛选 `Sokoban` 可重跑。上述结果对应本次 UI 重构后 2026-09-29 的报告 `Saved/Automation/UIRefactor/index.json`（生成于 UTC 10:40:22）；该报告属于本地生成文件，不纳入源码提交。现有四个 WBP 另经内存编译检查通过，Content 资产未改写。

存档测试使用独立随机槽位完成真实磁盘读写；PIE 测试在默认地图和蓝图控制器中注入临时 WBP，调用蓝图公开的 RequestAction，验证选关、结算、下一关及记录恢复。测试不会保存临时 WBP，也不代表用户界面的按钮已经接线。四个 WBP 的视觉与按钮接线、编辑器 DA 保存对话框、真实鼠标键盘输入、Shipping 打包和全新机器验收仍需独立验证。

## 7. 后续交付范围

- 内容与表现：多关难度节奏验证、美术替换和界面视觉打磨。主菜单、选关、结算、下一关与成绩存档已有实现。
- 范围约定：不做关卡解锁，不保存中途棋盘；暂停菜单与更丰富的存档迁移策略可按后续需求增加。
- 操作与反馈：按住连走／输入缓冲、音效、UI 视觉与交互打磨、美术替换。
- 工具扩展：编辑器内试玩入口、更多批量编辑能力，以及按需增加死锁提示或可解性检查。

当前灰盒使用引擎基础几何体和项目默认材质，不需要额外购买美术资源即可验证玩法。正式美术阶段需要地板、墙、箱子、箱子到位状态、目标标记、玩家表现及 UI／音效资源；应保持网格尺寸、物体中心点和目标可辨识性的一致性。

## 8. GitHub 提交要求

提交 `Source/`、`Content/`、`Config/`、`Scripts/`、`Docs/`、`Sokoban.uproject`、本 README 与 `.gitignore`。其中 `.uasset`、`.umap` 属于必要源资源，尤其不能遗漏 `Content/Sokoban/Materials/M_SokobanDefault.uasset`。

不提交 `Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/` 和 IDE 缓存。接收方应能从源码重新生成工程并编译；不要依赖旧工程的 DLL。仓库中的迁移重定向仍应保留，除非已经完成相应资产引用审计。
