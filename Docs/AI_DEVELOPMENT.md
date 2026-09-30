# AI 开发约定：推箱子框架接口与边界

用途：后续 AI Coding 开始任务前的简要上下文。基线：UE 5.6，2026-09-29。修改前核对实际源码；接口或约束变化时同步更新本文，不把“计划功能”当作已实现能力。

## 1. 目录与依赖

- 当前工程是 `Sokoban.uproject`。运行时核心位于 `Source/Sokoban/Sokoban/`，导出宏为 `SOKOBAN_API`；不依赖原 TopDown 模板角色和控制器。
- `Source/SokobanEditor/` 为仅编辑器使用的模块。依赖方向是 **Editor → Runtime**，禁止 Runtime 引用 UnrealEd、ContentBrowser 或关卡编辑面板。
- 数据流：`Enhanced Input → PlayerController → GameMode → Session → Rules`；成功通知再经 GameMode 更新 BoardActor。新 UI 的游戏操作也应通过 GameMode。
- 制作流：`Slate 临时选项 → Document 草稿 → DA 内存对象 → 保存资产包`；试玩读取 DA 的 Definition 副本，不回写设计资产。

## 2. 主要接口与调用约定

下表是入口速查，精确签名与类型以链接的头文件为准。优先复用现有接口，不另建一套推动判定或状态来源。

| 层／源码 | 主要接口 | 合同与边界 |
|---|---|---|
| [数据类型](../Source/Sokoban/Sokoban/Data/SokobanTypes.h)、[关卡 DA](../Source/Sokoban/Sokoban/Data/SokobanLevelData.h) | `FSokobanLevelDefinition`、`FSokobanBoardState`、`FSokobanMoveRecord`、`USokobanLevelData` | 分别保存初始布局、当前状态、一步变化、设计资产；当前箱子位置查 State，不查初始 Boxes |
| [Validator](../Source/Sokoban/Sokoban/Rules/SokobanLevelValidator.h) | `Validate(Level) → Issues` | 不修改数据；只检查结构，不证明可解或无死锁 |
| [Rules](../Source/Sokoban/Sokoban/Rules/SokobanRules.h) | `TryBuildMove(Level, State, Direction) → Result`；`ApplyMove`／`RevertMove → bool`；`IsSolved → bool` | 规划不改状态；仅 Walk／Push 的 Record 可用；执行／还原失败时 State 不变；不处理输入、动画、历史栈 |
| [Session](../Source/Sokoban/Sokoban/Gameplay/SokobanSession.h) | `Initialize(Level, OutIssues)`；`RequestMove`；`Undo`／`Redo`／`Restart`；`GetState`／`GetLevelDefinition` | 拥有状态与历史，读取返回副本。初始化失败保留旧局；仅游戏线程调用；由拥有者通过 UPROPERTY 持有 |
| Session 通知 | `OnSessionChangedNative()`／`OnSessionChanged`，参数 `FSokobanSessionUpdate` | 状态、历史、通关标志更新后广播，C++ 在蓝图之前；回调内禁止同步嵌套修改本局，可查询或安排后续操作 |
| [GameMode](../Source/Sokoban/Sokoban/Gameplay/SokobanGameMode.h) | `InitializeLevel`；`RequestMove`／`RequestUndo`／`RequestRedo`／`RequestRestart`；`IsReady`／`IsPresentationBusy`／`IsCompletionVisible` | 统一游戏操作入口，处理接入和动画限制。不要通过直调 Session 绕过动画限制 |
| [BoardActor](../Source/Sokoban/Sokoban/Presentation/SokobanBoardActor.h) | `BuildBoard`／`SynchronizeState`／`PresentMove(Record, bReverse, FinalState)`；`GridToWorld` | 只显示结果，不用碰撞或 Actor 位置裁定规则；Undo 传原正向记录并反向播放 |
| [编辑文档](../Source/SokobanEditor/Private/SokobanEditorDocument.h) | `NewLevel`／`Load`／`ApplyCell`／`Resize`／`Commit`／`Undo`／`Redo`／`Validate` | 操作私有快照，不直接写 DA；`FromAsset`／`WriteTo` 负责快照与资产字段转换 |
| [Slate 面板](../Source/SokobanEditor/Private/SSokobanLevelEditor.cpp) | 内部 `ApplySelectedCell`／`Save`／`ResolvePendingCell` | 面板负责交互、资产创建及包保存；内部方法不是运行时接口。`AcceptSaved` 只应在保存成功后接受新基线 |

## 3. 必须保持的边界

1. **坐标与占用：**网格 X 向右、Y 向下；Up 为 `(0,-1)`，地形下标为 `Y * Width + X`。先检查边界再访问数组。地形只含 Void／Floor／Wall，玩家、箱子和目标必须在 Floor；目标可与玩家或箱子共存，玩家与箱子、箱子之间不可重叠。
2. **身份与数量：**BoxId 非负且本关唯一；目标坐标唯一，箱子／目标非空且数量相等。LevelId 是项目内关卡身份，不是文件路径。布局校验器不检查跨资产 ID，ID 重复由编辑面板检查。
3. **行动与失败：**每次只走一格，最多推动一个箱子，不允许拉箱或推一串箱子。成功移动 MoveCount 加一，推动额外增加 PushCount；失败不改位置、计数和历史。保留非法状态与整数溢出的检查。
4. **历史：**新移动成功才清空重做分支；Undo／Redo 成功才转移记录；Restart 恢复 InitialState 并清空历史。记录只描述相关实体与计数，不是整局身份签名，历史归属由 Session 管理。
5. **通关与时序：**Rules 不锁定已通关状态，Session 拒绝通关后的新移动，但允许撤销／重开。逻辑结果先提交，动画随后播放；完成界面应查询 `IsCompletionVisible()`，等待最后一步动画结束。
6. **输入与场景：**相同方向持续按住只触发一步，动画期间输入被忽略，目前无排队缓冲。场景零个 BoardActor 时自动生成，一个时复用，多个时报错；箱子是网格组件，不要求独立 Actor。
7. **关卡加载：**正式模式从 LevelCatalog 选关，先校验目录和布局。旧单关调试中，指定的 StartupLevel 非法时报告失败；只有引用为空且允许示例时才回退到内置关卡。保存 DA 不自动指定 StartupLevel，也不热更新已开始的 Session。
8. **工具约束：**最大 64×64，最近 128 次编辑快照；缩小保留左上角重叠区并裁掉范围外内容，新格为 Floor。工具历史独立于游戏历史和 UE 场景全局撤销。PIE 时禁用编辑。
9. **保存语义：**Pending 不等于已应用草稿，草稿不等于资产文件。允许保存结构不完整的草稿，但试玩前必须校验；缺失／重复 ID 等仍可阻止保存。`MarkPackageDirty`／`PostEditChange` 不等于磁盘保存成功。
10. **保存失败与版本：**包保存未成功时恢复资产内存字段、保留草稿，不承诺完整磁盘事务回滚或删除已新建资产。外部资产变化需提示覆盖，不自动合并。当前工具只接收 DataVersion=1；已有资产布局变化时递增 ContentRevision；两种版本不可混用。

## 4. 扩展落点

| 需求 | 应同步检查／修改 |
|---|---|
| 新地形、箱子能力或推动规则 | Types → Validator → Rules → Session 初始化／历史记录 → 编辑文档与 Slate 选项 → Board 表现；保证可撤销或明确调整历史机制 |
| 连走、输入缓冲或其他输入设备 | PlayerController／GameMode 调度，最终调用既有操作入口，不在输入绑定中复制规则 |
| 音效、特效、正式 UI | 消费成功通知和表现状态；规则失败不得播放成功反馈，结算注意动画结束时机 |
| 选关、下一关、存档 | 增加流程／关卡列表／持久化层；存档关联 LevelId、内容修订及结构版本，加载前校验；不要写回设计 DA |
| 新增可编辑关卡字段 | DA 与 Types、Snapshot 的 FromAsset／WriteTo／Equals、控件提交和撤销、校验、版本兼容及测试 |

当前已实现主菜单／选关／正式结算与成绩存档。范围外：关卡解锁、中途棋盘存档、自动求解／死锁判定、编辑器内一键试玩、草稿崩溃恢复。GetPrimaryAssetId 不代表自动扫描关卡；正式目录采用显式硬引用。

## 5. 修改与验证要求

- 不随意重命名反射类型、属性、枚举或移动资产；确有需要时处理兼容与引用。保留现有 CoreRedirects，移除前先审计。必要 `.uasset`／`.umap` 属于源资源；生成目录不提交。
- 改规则或状态管理时，验证失败无副作用、推箱阻挡、撤销／重做分支、重开、通关切换和通知重入；改工具时检查 Pending、保存取消／失败、ID 冲突、裁剪及编辑历史。
- 基线自动化组为 `Sokoban.Validation`、`Rules`、`Session`、`Gameplay`、`Editor`、`Frontend`，现有共 42 项。按改动运行相关测试；涉及反射／模块时完整编译 Editor，变更模块依赖时同时检查 Game 目标。测试报告必须区分本次执行与历史结果。
- 保存对话框和真实磁盘保存需专项验收，现有内存副本测试不能替代；不要据此宣称完整打包或跨机器运行已验证。新增接口和行为应有必要的中文注释，并同步本文及受影响的操作文档。

更多操作细节见[工程配置](../Source/Sokoban/Sokoban/EditorSetup.md)和[关卡制作规范](LevelEditor.md)。

## 6. 正式流程新增约定

- `USokobanLevelCatalog` 保存有序 DA 引用；`USokobanFlowSubsystem` 提供 ShowMainMenu／ShowLevelSelect／StartLevel／Replay／NextLevel，状态为 MainMenu、LevelSelect、Playing、Results。
- `USokobanProgressSubsystem` 负责 Save／RecordCompletion／GetProgress／RecoverSaving；`USokobanSaveGame` 保存载荷。最佳成绩按 MoveCount 优先、PushCount 次之，始终成对保存；只查询相同 ContentRevision 的成绩。
- 默认开启 GameMode.EnableFrontend，按 LevelCatalog 选关；StartupLevel 仅供关闭正式流程后的单关调试。正式模式下 InitializeLevel 由 Flow 开始关卡期间调用，不能从 UI 直接绕过。
- 正式结算在动画结束后由 GameMode Tick 触发 Flow.TryComplete，每局仅一次；结算后 GameMode 屏蔽移动、撤销、重做和原 Restart 入口，重玩改走 Flow.Replay。底层 Session 保留原能力。
- Flow 只弱引用 GameMode，GameMode.EndPlay 中 Detach。`USokobanUIManagerComponent` 由控制器持有，负责创建已配置的四种 WBP、解绑、移除控件及输入焦点；同页通知不得重建界面；Runtime 的 UMG／Slate 渲染依赖合法，仍不得引用编辑器工具模块。
- 读取异常存档时不自动覆盖；RecoverSaving 必须经用户确认，备份失败不继续覆盖。保存失败不阻断结算，内存记录保留。自动化使用 SokobanAutomation_ 前缀随机槽位，不写正式玩家存档。
- 新增验证组 Sokoban.Frontend，详见[正式流程接入](Frontend.md)。

## 7. 运行 UI 的硬边界

- WBP 独占控件树、布局、字体、颜色、动画与交互表现。禁止在运行 C++ 中 ConstructWidget、RebuildWidget 造界面、硬编码控件名称或 BindWidget 结构。Slate 关卡编辑器与旧单关 Canvas 调试 HUD 不在这次运行 UMG 重构范围。
- `USokobanScreen` 提供 `GetViewData`、`GetLevelEntries`、`OnViewDataChanged` 和 `RequestAction`；四个派生类只作 WBP 父类，原生类为抽象类。未配置 WBP 需报告错误，禁止默认布局兜底。
- 游戏命令经 GameMode，页面命令经 Flow；快照的按钮可用状态只作展示，后端校验不能删除。蓝图更新数据事件内不得发命令；事件有防重入保护。
- Repair／未保存 Quit 通过 `OnConfirmationRequested` 通知 WBP，用户确认或取消后调用 `ResolveConfirmation`；重复请求不等于同意。移除界面后旧回调失效。
- 旧 `Execute(Action, Index, Source)` 保留兼容，Source 无显示含义；新按钮使用 RequestAction。GetLevelEntries 保留目录索引，不能把排序后的行号用于开始关卡。
- WBP 保持 Tick Frequency=Auto，用于自动同步动画锁和快捷键状态；隐藏／禁用 Tick 时改由蓝图在恢复显示后调用 RefreshViewData(true)。详细接线见 BlueprintUI.md。
