# 工程配置与试玩接入规范

适用对象：程序、技术策划，以及负责在本机恢复工程的协作者。本文负责“环境、资产引用、运行接入和排障”；日常关卡布局制作见[关卡制作与交付操作规范](../../../Docs/LevelEditor.md)。

适用版本：UE 5.6；文档核对日期：2026-09-29。文中的资产路径对应当前工程。已有资产应优先复用，只有缺失或需要重建时才执行配置章节。

## 1. 环境与首次启动

### 1.1 环境要求

| 项目 | 要求／已验证配置 |
|---|---|
| 引擎 | 工程关联 UE 5.6；本地验证使用 UE 5.6.1 |
| 平台与工具链 | Windows；已验证 Windows 11、VS 2022、MSVC 14.38、SDK 10.0.22621 |
| 输入插件 | Enhanced Input，已在 `.uproject` 启用 |
| 模块 | `Sokoban` 为 Runtime，`SokobanEditor` 为 Editor |
| 编译目标 | `SokobanEditor / Development Editor / Win64` |
| 必须取得的文件 | `Source`、`Content`、`Config`、`Sokoban.uproject`；提交规范见[项目 README](../../../README.md) |

右键 `.uproject` 生成工程文件，随后在 IDE 中编译 Editor 目标并打开工程。首次缺少模块时也可使用 UE 提示的重新编译。新增或修改反射类型、模块配置后，应保存并关闭 UE，再完整编译；普通文档修改不需要关闭编辑器。

如果本机只有 Rider，仍需安装 UE 所需的 C++ 编译工具链。不要复制其他工程的 DLL 来代替编译。

### 1.2 已有工程的最短试玩流程

1. 打开 `Sokoban.uproject` 和 `/Game/Sokoban/L_SokobanPlayerGround`。
2. 确认 World Settings 或项目默认 GameMode 使用 `BP_SokobanGameMode`，其 Player Controller Class 使用 `BP_SokobanPlayerController`。
3. GameMode 开启 `Enable Frontend`，`Level Catalog` 指定 `/Game/Sokoban/Levels/DA_SokobanCatalog`。
4. 按 [UMG 蓝图接入](../../../Docs/BlueprintUI.md) 完成四个 WBP 的布局、事件与 UIManager 页面配置，然后点击 Play，通过主菜单选择关卡。正式流程和存档说明见[正式流程接入](../../../Docs/Frontend.md)。
5. 如需旧单关调试，关闭 `Enable Frontend` 后才使用 `Startup Level`；其为空且启用 `Use Demo Level If Unset` 时使用内置示例。

DA 必须加入目录并通过校验才能正常开始。修改关卡后重新 Play；保存资产不热更新正在运行的会话。

## 2. 资产与配置对照表

以下均为内容浏览器中的包路径，实际文件位于 `Content/`：

| 资产 | 当前路径 | 作用 |
|---|---|---|
| 试玩地图 | `/Game/Sokoban/L_SokobanPlayerGround` | 加载世界与游戏模式；注意路径拼写为 `PlayerGround` |
| GameMode 蓝图 | `/Game/Sokoban/Blueprints/BP_SokobanGameMode` | 指定控制器、初始关卡和棋盘类 |
| 控制器蓝图 | `/Game/Sokoban/Blueprints/BP_SokobanPlayerController` | 配置 IMC 与四个 Input Action |
| 输入映射 | `/Game/Sokoban/Input/IMC_Sokoban` | 按键到动作的映射 |
| 输入动作 | `/Game/Sokoban/Input/IA_SokobanMove`、`IA_SokobanUndo`、`IA_SokobanRedo`、`IA_SokobanRestart` | 移动、撤销、重做、重开；后三者位于相同目录 |
| 现有关卡数据 | `/Game/Sokoban/Data/DA_SokobanLevelData` | 可在工具中打开检查；接入前必须校验 |
| 默认材质 | `/Game/Sokoban/Materials/M_SokobanDefault` | 棋盘各类物体的默认着色基础 |

`DefaultEngine.ini` 配置默认地图与 GameMode；`DefaultInput.ini` 配置 Enhanced Input 类；`DefaultGame.ini` 包含试玩地图的烘焙配置。世界设置覆盖项优先于项目默认项，排障时应同时检查。

## 3. 输入配置规范

### 3.1 Input Action

| 资产名 | Value Type | 控制器属性 |
|---|---|---|
| `IA_SokobanMove` | Axis2D / Vector2D | Move Action |
| `IA_SokobanUndo` | Digital / Bool | Undo Action |
| `IA_SokobanRedo` | Digital / Bool | Redo Action |
| `IA_SokobanRestart` | Digital / Bool | Restart Action |

四个属性必须引用不同的 Input Action，且都要在 `IMC_Sokoban` 中至少有一条映射。当前控制器会检查资产缺失、动作类型和映射缺失，并反馈配置错误。

### 3.2 IMC 按键映射

同一行列出的不同按键需分别建立映射。方向修饰器添加在各条按键映射上，Input Action 本身无需统一添加这些修饰器。

| Action | 按键 | Modifiers，按顺序配置 |
|---|---|---|
| Move | D、Right | 无 |
| Move | A、Left | Negate，只反转 X |
| Move | W、Up | Swizzle Input Axis Values，Order = YXZ |
| Move | S、Down | 先 Negate，只反转 X；再 Swizzle，Order = YXZ |
| Undo | Z | 无 |
| Redo | Y | 无 |
| Restart | R | 无 |

键盘单键初始输入为正 X；上述配置使 W 得到 `(0,1)`，S 得到 `(0,-1)`。控制器再把正输入 Y 转成网格的 Up，即格子坐标 Y 减一。

不需要额外添加 Hold／Pressed Trigger。移动绑定 `Triggered`，释放绑定 `Completed`／`Canceled`；控制器会抑制相同方向的持续触发。撤销、重做和重开绑定 `Started`。

### 3.3 控制器与项目设置

`BP_SokobanPlayerController` 的父类应为 `ASokobanPlayerController`。在 Class Defaults 的输入配置中填入上述 IMC 和四个 Action，编译并保存蓝图。

Project Settings → Input 中，Default Player Input Class 应为 `EnhancedPlayerInput`，Default Input Component Class 应为 `EnhancedInputComponent`。代码在本地玩家初始化时添加 IMC，并在结束时移除自己添加的映射。

当前输入没有按住连走和排队缓冲。若在移动动画期间按键，该次请求被忽略；松开后重新按下。不要将其误判为输入资产没有绑定。

## 4. GameMode、关卡与场景接入

### 4.1 GameMode 配置

`BP_SokobanGameMode` 的父类应为 `ASokobanGameMode`。默认 Pawn、HUD 和棋盘类由 C++ 提供：`ASokobanCameraPawn`、`ASokobanHUD`、`ASokobanBoardActor`。玩家显示使用棋盘中的网格组件，Pawn 负责相机，不需要再配置 TopDown 模板 Character。

| 字段 | 配置规则 |
|---|---|
| Player Controller Class | 指向已配置输入的 `BP_SokobanPlayerController` |
| Startup Level | 需要试玩的 `USokobanLevelData` 资产；更换后编译、保存蓝图并重新 Play |
| Use Demo Level If Unset | 仅在 Startup Level 为空时启用内置示例 |
| Board Class | 场景没有棋盘实例时生成的类；可使用默认类或自定义派生蓝图 |

正式模式将 DA 加入 Level Catalog 后从选关页进入；Startup Level 仅在关闭 Enable Frontend 的单关调试模式使用。关卡编辑器保存 DA 不自动修改这两处引用，也不会热替换正在运行的 Session。

### 4.2 棋盘摆放

场景中可以不放 BoardActor，此时 GameMode 自动生成；也可以放置一个 `ASokobanBoardActor` 或其派生蓝图，配置位置、颜色和材质。场景里存在多个棋盘实例会导致接入失败，应只保留一个。

箱子、玩家、地板、墙和目标依据布局在运行时创建，不需要逐个摆放 Actor。`GridToWorld` 将格子坐标转换到棋盘局部坐标，再叠加棋盘变换；关卡数据中的 `(X,Y)` 不是世界坐标。相机自动适配棋盘范围。

### 4.3 关卡资产

优先使用“窗口 → 推箱子关卡编辑器”创建、校验并保存 DA。工具新建保存对话框默认目录为 `/Game/Sokoban/Levels`，并不要求所有关卡都必须在该目录。

需要直接检查 DA 时，可查看 `Definition`：

- `Width`、`Height` 是格子数，`Terrain` 数量必须等于宽乘高，下标为 `Y * Width + X`。
- 地形包括 Void、Floor、Wall；目标、玩家和箱子只能位于 Floor。
- `Goals` 保存目标坐标；`PlayerSpawn` 保存唯一出生点；`Boxes` 保存 ID 与初始坐标。
- 箱子 ID 非负且本关唯一；箱子和目标均非空、数量相等；箱子不得与其他箱子或玩家重叠。
- 玩家或箱子可以位于目标点上。地图不强制封闭围墙，但越界移动会被拒绝。

直接编辑数组适用于数据检查或排障，不建议作为日常制作入口。校验器检查结构，不判断谜题是否可解。

## 5. 显示、颜色与资源替换

默认使用引擎 Cube、Sphere、Cylinder：墙和箱子为方块，玩家为球体，目标为扁平圆柱。默认色彩语义为：地板灰色、墙深蓝、目标黄色、普通箱子橙色、到位箱子绿色、玩家蓝色。

在棋盘实例或派生蓝图中可以配置 `Cell Size`、`Move Duration`、六类 `*Color` 与 `*Material`。默认格子尺寸为 150，移动动画为 0.14 秒。自定义材质优先于对应的颜色；改变颜色不会强制修改已指定的材质。

`Box Material` 和 `Box On Goal Material` 是不同配置项。只指定普通箱子材质，不会自动覆盖到位状态的材质。目标的默认圆盘大于箱子底面，用于保持到位区域可辨识。

未指定覆盖材质时，代码使用 `M_SokobanDefault` 创建动态材质实例，参数为 `BaseColor` 和 `EmissiveStrength`。必须提交该 `.uasset`。仓库中的[材质创建脚本](../../../Scripts/Editor/CreateSokobanDefaultMaterial.py)仅用于需要重建材质时的开发辅助；正常运行已有工程无需执行脚本或启用 Python 插件。

正式替换网格前，应统一尺寸、中心点和高度约定。当前网格选择由 C++ 实现，不能把替换材质等同于已经完成任意模型的配置接入。

## 6. 首次接入验收

以下为关闭 Enable Frontend 后的单关调试验收，保留通关后撤销能力。正式流程应通过选关开始，结算后禁用撤销／重做／R，改用“重新挑战”，详见[正式流程验收](../../../Docs/Frontend.md)。

| 步骤 | 预期结果 |
|---|---|
| Play | 7×5 棋盘；玩家 `(2,3)`；两箱 `(2,2)`、`(4,2)`；目标 `(2,1)`、`(4,1)` |
| 每次动画完成后依次 W、S、D、D、W | 5 步、2 次推动，两个箱子到位后显示通关 |
| 通关后按 Z | 最后一推撤销，位置和计数恢复，通关状态解除 |
| 按 Y | 重做最后一推，动画结束后再次显示通关 |
| 按 R | 恢复初始位置，步数／推动数归零，历史清空 |
| 尝试撞墙／连续推两个箱子 | 行动被拒绝，位置与计数不变化 |
| 保存自制 DA 并指定 Startup Level 后重新 Play | 显示自制布局；停止游戏后 DA 初始坐标不变 |

自动化测试入口与已验证范围见[项目 README](../../../README.md)。本流程是本机接入验收，不能替代正式打包与全新机器验收。

## 7. 常见故障排查

| 现象 | 优先检查与处理 |
|---|---|
| 找不到关卡编辑器菜单 | 确认完整编译的是 SokobanEditor 目标；`.uproject` 包含 Editor 模块；关闭并重新启动 UE |
| 启动提示缺少模块或模块版本不同 | 确认引擎版本、C++ 工具链和源码完整，关闭 UE 后重新编译；不要复用旧 DLL |
| 没有棋盘或初始化失败 | 查看 HUD／Output Log；检查 Startup Level 校验结果、是否存在多个 BoardActor |
| WASD 无响应 | 游戏视口焦点、实际 GameMode／Controller 类、IMC 与四个 Action、EnhancedInputComponent 配置 |
| W／S 方向错误 | 检查映射上的 Negate、Swizzle 顺序，避免把修饰器重复加到 Action 上 |
| 只走一步或快速按键被忽略 | 当前输入设计如此：无连续走、无动画期间输入缓冲 |
| 改了关卡但试玩仍是旧布局 | 工具中应用格子并保存资产，检查 Startup Level 是否引用正确 DA，停止后重新 Play |
| 物体全白或目标难以识别 | 默认材质是否存在；覆盖材质是否遮盖颜色设置；目标与到位箱子是否分别配置 |
| 关卡编辑器整窗不可用 | 停止 PIE；当前工具在 PlayWorld 存在时禁用编辑 |
| 编译后类未出现在 IDE 索引 | 重新生成工程并刷新 IDE；以真实编译结果判断模块是否有效，索引状态不是编译结果 |

## 8. 协作交付约定

新增玩法源文件放在 `Source/Sokoban/Sokoban/` 对应职责目录；编辑器专用代码放在 `Source/SokobanEditor/`。不要让 Runtime 模块依赖 Slate 工具或 UnrealEd。

提交关卡修改时一并提交 DA；修改试玩入口时一并提交相关蓝图或配置。保留 `Config` 中现有迁移重定向，删除前需验证资源不再引用旧模块。提交前从版本控制变更列表排除生成目录与 IDE 缓存，并检查必要 `.uasset`、`.umap` 没有遗漏。

## 运行界面的蓝图接入

运行 UMG 的布局与样式由四个 WBP 完全负责，C++ 不再提供默认布局。先按 [UMG 蓝图接入](../../../Docs/BlueprintUI.md) 在 BP_SokobanPlayerController 的 UIManager 组件配置页面类，再实现数据显示和按钮事件。现有 WBP 父类正确，无需重建资产。缺少页面配置时看 Output Log 或 GetConfigurationError；测试中的临时布局不会保存到 Content。
