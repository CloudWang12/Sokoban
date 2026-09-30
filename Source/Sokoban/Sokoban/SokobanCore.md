# Sokoban 核心代码

当前属于 Sokoban 运行时模块，不是独立模块。现阶段已有数据、校验、规则、单局管理，以及 Enhanced Input、棋盘显示、动画、镜头和灰盒 HUD。编辑器工具位于独立的 SokobanEditor 模块；正式主菜单、选关、成绩存档与结算已加入，入口见[正式流程](../../../Docs/Frontend.md)。

首次试玩的编辑器配置见 [EditorSetup.md](EditorSetup.md)。

## 目录职责

- Data：关卡定义、棋盘状态、操作记录、关卡数据资产。
- Rules：移动与推动规则、通关判断、关卡数据校验。
- Gameplay：单局管理、撤销、重做、重开、GameMode 和输入控制器。
- Presentation：棋盘组件、移动动画、镜头 Pawn 和灰盒 HUD。
- Flow：主菜单、选关、游戏和结算的状态协调。
- Progress：最佳成绩与存档读写、版本检查及恢复。
- UI：数据快照、蓝图命令与控制器持有的页面管理组件；视觉布局仅在 WBP 中制作。
- Tests：校验、规则、单局、玩法和正式流程测试。

## 命名与边界

- 自有文件与类型使用 Sokoban 命名，例如 SokobanRules.h、FSokobanRules、USokobanSession。
- 需要导出到其他模块的类型仍使用 SOKOBAN_API 宏，该宏对应所属模块。
- 格子规则不依赖 Actor、输入、动画或编辑器 API。
- Unreal 编辑器工具位于独立的 SokobanEditor 模块，不放入本运行时目录。
- 模块入口和构建配置位于上一级目录；本工程不包含旧项目的模板玩法代码。

## 已有数据定义

- `Data/SokobanTypes.h`：地形、方向、行动结果、箱子初始数据、关卡布局、棋盘状态、计数、行动记录、校验问题。
- `Data/SokobanLevelData.h/.cpp`：可由内容浏览器创建的 `USokobanLevelData` 数据资产。

坐标采用左上角为原点的二维网格：X 向右增加，Y 向下增加，向上是 `(0, -1)`。地形按行展开，索引为 `Y * Width + X`；空洞和墙不可站立。BoardActor 映射到局部坐标 `(-Y * CellSize, X * CellSize, Height)`，再应用 Actor 变换，与默认镜头的屏幕方向一致。

目标点、地形、箱子占用分别保存。初始布局中的箱子数组允许校验器发现重复 ID；运行时状态按唯一箱子 ID 保存位置。玩家出生点和未初始化位置使用 `(-1, -1)`，未分配箱子 ID 使用 `INDEX_NONE`。

空的默认关卡表示未完成草稿，不自动生成可玩关卡。属性面板的范围提示不是数据校验；关卡开局前应调用校验器检查尺寸、数组长度、坐标、数量、ID 和重叠。

一次推动记一次移动和一次推动。行动记录用 `BoxId != INDEX_NONE` 表示推动，并记录操作前后的计数；普通移动不使用箱子坐标字段。行动结果仅在 `Walk` 或 `Push` 时携带可执行记录。

关卡资产的 `LevelId` 必须手动填写且在项目中唯一，例如 `Chapter01_Level01`。它构成 `SokobanLevel:<LevelId>` 主资产 ID，不随文件改名而变化；复制为新关卡时必须更换 ID。尚未配置 ID 的草稿返回无效主资产 ID。目录级 ID 校验已经由 USokobanLevelCatalog 实现；仍未配置 Asset Manager 自动扫描，目录采用显式引用。

`DataVersion` 表示数据格式版本，`ContentRevision` 表示谜题内容修订号。修改谜题后应增加修订号，后续存档恢复和解法验证需要同时核对关卡 ID 与修订号。

用于棋盘和操作历史的属性已标记 `SaveGame`，但当前不保存中途棋盘或历史。已实现的存档仅记录关卡成绩和最近关卡，载荷是 USokobanSaveGame。

## 关卡结构校验

入口为 `FSokobanLevelValidator::Validate(Level)`，返回 `TArray<FSokobanValidationIssue>`，不会修改输入数据，也不需要启动场景。当前问题全部为 `Error`；后续增加警告时，调用者应按严重程度判断是否允许开局。

校验包含尺寸与乘法溢出、地形数组长度与枚举有效性、出生点、箱子 ID、各实体的坐标与地板要求、目标去重、箱子与目标数量、箱子之间及玩家与箱子的占用冲突。玩家或箱子与目标点重合合法，不强制关卡边缘必须是墙。

尺寸或地形数组不合法时，跳过依赖它们的不安全读取，继续报告可独立判断的问题。问题的 `Code` 用于程序判断，中文 `Message` 用于显示，`Cells` 用于编辑器高亮；调用者应过滤越界坐标后再绘制。

校验通过仅代表布局数据合法，不保证可解，也不检查项目范围的关卡 ID 唯一性。

自动化测试位于 `Tests/SokobanLevelValidatorTests.cpp`，在 UE 自动化测试窗口中筛选 `Sokoban.Validation` 可运行，覆盖合法布局、尺寸与数组边界、非法地形、出生点、箱子、目标、多问题报告及输入保持不变。

## 移动与推动规则

`FSokobanRules` 提供四个接口：

- `TryBuildMove(Level, State, Direction)`：只检查并返回行动结果；成功时生成记录，不修改棋盘。
- `ApplyMove(Level, State, Record)`：重新生成并核对合法记录，更新玩家、可选箱子与计数。
- `RevertMove(Level, State, Record)`：核对操作后位置和计数，在副本上恢复并验证前一状态，然后整体提交。
- `IsSolved(Level, State)`：仅在关卡和当前状态都合法且所有箱子都在目标点时返回 true。

接口会检查关卡结构、当前箱子 ID 集合、占用、地板要求及计数。无效输入方向返回 `InvalidDirection`；非法数据或无法继续递增的计数返回 `InvalidState`。玩家或被推箱子出界返回 `OutOfBounds`，遇到墙或空洞返回 `BlockedByTerrain`，箱子后方还有箱子返回 `BlockedByBox`。失败结果不携带有效记录，执行或撤销失败时状态完全不变。

记录匹配检查涉及本次移动的角色、箱子位置和前后计数，不是整个棋盘的历史身份凭证。Session 在所属单局的私有撤销／重做栈中使用记录；切关、重开、分支操作时维护或清空历史，外部不应任意混用其他单局的记录。

规则层不处理输入重复、动画等待或通关后的操作锁定；即使棋盘已满足通关条件，规则仍能计算下一步。Session 决定是否接受行动。初版每次调用都会验证数据，优先确保正确性；如未来用于大量求解搜索，再引入经过验证的关卡上下文减少重复检查。

自动化测试筛选 `Sokoban.Rules`，覆盖四方向、推动与撤销／重做、不能拉箱子、地形与边界阻挡、连续箱子、非法状态、损坏或不匹配的记录、计数溢出、通关判断，以及固定种子的 256 次输入序列整体撤销和重做。筛选 `Sokoban` 可同时运行校验器与规则测试。

## 单局管理

`USokobanSession` 是 UObject，使用 `NewObject<USokobanSession>(Owner)` 创建，并由拥有者通过 `UPROPERTY` 持有以管理生命周期。它保存关卡副本、初始快照、当前状态和两个私有历史栈。

- `Initialize(Level, OutIssues)`：校验成功后复制关卡、生成初始状态并清空历史。失败返回问题列表，原有对局不变。
- `RequestMove(Direction)`：调用 Rules 规划和执行；成功后加入撤销栈并清空重做分支。失败输入不改变棋盘和历史。
- `Undo()` / `Redo()`：先让 Rules 成功还原或执行，再转移对应记录。
- `Restart()`：恢复初始快照，清空两个历史栈。
- `GetState()` / `GetLevelDefinition()` 返回副本；外部不能通过这些查询接口直接修改内部数据。
- `IsInitialized()` / `IsSolved()` / `CanUndo()` / `CanRedo()` 及历史数量查询供流程与 UI 使用。

通关后停止普通移动，但允许撤销、重做和重开。初始化时箱子已全部到位的关卡也会立刻被标记为通关。未初始化、通关锁定或通知执行期间的移动请求返回 `InvalidState`；其他失败原因保留 Rules 的返回结果。

成功操作统一发出 `FSokobanSessionUpdate`：`Change` 区分初始化、移动、撤销、重做、重开；单步操作附带原始行动记录，撤销由表现层按原记录反向播放；`bIsSolved` 与 `bSolvedChanged` 描述通关状态。初始化和重开不携带有效单步记录，应读取当前状态重建或同步整个棋盘。

蓝图绑定 `OnSessionChanged`，C++ 使用 `OnSessionChangedNative().AddLambda/AddUObject`。两个入口接收同一份通知，先通知 C++、再通知蓝图。通知期间棋盘和历史已更新，可以查询，但所有嵌套修改请求会被拒绝；需要连走或切关的监听者应在通知结束后提交下一次操作。这里的同步保护不是动画锁；动画锁由 GameMode 负责，输入缓冲和暂停尚未实现。

自动化测试筛选 `Sokoban.Session`，覆盖初始化和副本隔离、连续撤销／重做、分支清空、失败输入、重开、切关、通关变化、通知一致性和回调重入保护。筛选 `Sokoban` 可运行目前全部推箱子测试。

## 可试玩的场景连接

调用顺序为 `Enhanced Input → ASokobanPlayerController → ASokobanGameMode → USokobanSession → FSokobanRules`。成功提交后，Session 通知 GameMode，后者让 BoardActor 展示结果。HUD 查询计数和流程状态。

- `ASokobanPlayerController`：把 Axis2D 输入转为四方向，把撤销／重做／重开转为请求；校验输入资产是否缺失、类型是否正确和是否具有按键映射。方向取较大分量，相等时取水平；每次按下或改变方向只走一步。
- `ASokobanGameMode`：持有 Session 和棋盘，初始化关卡，统一限制动画期间的操作。优先使用地图中唯一的 BoardActor，没有则按 BoardClass 生成。配置多个棋盘会拒绝开局。
- `ASokobanBoardActor`：以实例组件显示地板、墙、目标，以独立静态网格组件显示玩家和各箱子。按移动记录插值，结束时用最终状态精确对齐；重开直接同步。UE 物理碰撞不参与玩法判定。
- `ASokobanCameraPawn`：只负责镜头。根据棋盘范围和视口比例取景，并跟随棋盘整体变换；玩家棋子是 BoardActor 的组件，并非被控制的 Character。
- `ASokobanHUD`：仅在关闭正式流程的单关调试模式显示 Canvas 调试信息；正式模式使用策划制作的 WBP。

从界面或蓝图发起玩家操作时，调用 GameMode 的 `RequestMove/Undo/Redo/Restart`，不要直接修改 Session 或 Actor 坐标。`GetSession()` 用于读取与监听；直接调用它的写接口会绕过表现层的动画锁。

棋盘默认配色使用 `Content/Sokoban/Materials/M_SokobanDefault.uasset` 和运行时动态材质实例，不依赖编辑器代码。目标为黄色，普通箱子为橙色，到位箱子为绿色，玩家为蓝色。`Sokoban | Colors` 可调整默认颜色；`Sokoban | Materials` 的自定义材质优先。到位与普通箱子分别配置，动画结束、撤销和重开时按状态同步。基础材质需要随 Content 提交；`Scripts/Editor/CreateSokobanDefaultMaterial.py` 仅用于创建资产，日常编译运行不需要启用 Python 插件。

当前动画期间的输入直接丢弃，不排队；按住一个方向不会连续移动。Session 的通关状态在提交最后一步时即为 true，而 `GameMode::IsCompletionVisible()` 等待动画结束才返回 true。通关提示应使用后者。

没有指定 StartupLevel 且启用 `bUseDemoLevelIfUnset` 时，以 `MakeDemoLevel()` 的 7×5 示例开局。自定义关卡可设置 StartupLevel；正式关卡流程完成后可关闭示例回退。数据资产本身不会被此功能改写。

自动化测试筛选 `Sokoban.Gameplay`，覆盖移动与反向动画、操作锁、延迟通关提示、重开、零动画时长、组件重建、棋盘整体变换、横竖屏镜头范围、方向映射与输入资产配置。无界面测试不替代编辑器内的输入接线与视觉验收。
