# 运行 UMG：蓝图布局与 C++ 数据接入

## 1. 职责与当前状态

C++ 负责流程、游戏操作、存档、只读界面数据、页面实例和输入模式。Widget Blueprint 负责控件树、布局、文字排版、颜色、图标、动画、音效和确认弹窗。没有固定控件名称，也没有 `BindWidget` 命名约束。

项目现有四个 WBP 的父类已经正确，本次未改动这些资产。UIManager 原先引用原生页面类，需要按下表重新指定。移除 C++ 默认布局后，空白 WBP 会显示为空白，这是正常的；需要在 Designer 中制作界面并在 Graph 中接线。

## 2. 页面配置

打开 `BP_SokobanPlayerController`，选择 `SokobanUIManager` 组件，在 Details 的 Sokoban / UI 配置：

| 字段 | 选择资产 | WBP 的 C++ 父类 |
|---|---|---|
| Main Menu Class | `/Game/Sokoban/UI/WBP_MainMenu` | SokobanMainMenuWidget |
| Level Select Class | `/Game/Sokoban/UI/WBP_LevelSelect` | SokobanLevelSelectWidget |
| Play Class | `/Game/Sokoban/UI/WBP_PlayHUD` | SokobanPlayWidget |
| Results Class | `/Game/Sokoban/UI/WBP_Results` | SokobanResultsWidget |

编译并保存控制器和四个 WBP。无需在蓝图里再次 Create Widget / Add to Viewport；UIManager 在流程状态变化时创建正确页面，同页数据通知复用当前实例。类配置缺失或父类错误会记录 Output Log，并可通过组件的 `GetConfigurationError` 读取；不会生成 C++ 备用布局。

## 3. 数据显示

在四个 WBP 的 Graph 中添加 `Event On View Data Changed`。将 Data 拆分（Break Sokoban UI View Data），把字段显示到自己命名的控件。第一次 Construct 后会强制通知一次；之后 C++ Tick 比较快照，数据变化才再次通知。

| 数据 | 用途 |
|---|---|
| Available / State | 是否已接入有效游戏流程、当前页面状态 |
| Completion.DisplayName / LevelId / ContentRevision | 当前关卡身份；开始时冻结 |
| Counters.MoveCount / PushCount | 游戏实时计数，或结算时冻结的计数 |
| UndoCount / RedoCount | 可撤销、重做的历史数量 |
| Busy / CanUndo / CanRedo / CanRestart | 动画状态和按钮 Is Enabled |
| HasNextLevel | 结算页下一关按钮的显隐或可用状态 |
| HasBest / Best / Completion.NewBest | 最佳成绩及本次是否刷新纪录 |
| LastPlayedIndex | 最近关卡的目录索引；-1 表示无记录；从该关起点重玩 |
| Status / SaveStatus | 流程、操作或存档提示，由蓝图决定显示位置 |
| UnsavedChanges / WriteBlocked | 是否有未保存进度、异常存档是否处于写保护 |
| ConfirmationPending | 是否有等待蓝图确认的操作 |

需要主动读取时调用 `GetViewData`；重新绑定或恢复显示后可调用 `RefreshViewData(true)`。WBP 的 **Tick Frequency 保持 Auto**，不要禁用原生 Tick；否则快捷键与动画完成后的状态不会自动刷新。设计器预览没有运行世界时，Available=false；预览占位数据由蓝图 PreConstruct 自行设置。

`OnViewDataChanged` 只更新表现，不调用 RequestAction，避免数据通知又触发游戏操作形成循环。数字格式与中文提示标题由蓝图自己组合，C++ 不设置 TextBlock 的文字、颜色或字号。

## 4. 选关列表

在选关页 Construct 时调用 `GetLevelEntries`，For Each Loop 遍历返回的 `Sokoban Level List Entry`，由蓝图自行创建条目 WBP 并添加到 VerticalBox、WrapBox 或其他容器。

每个条目包含 Index、LevelId、DisplayName、ContentRevision、CanStart、Completed 和 Best。把条目数据保存在条目 WBP 变量中；条目点击时广播自己的事件，把 **Entry.Index** 交给选关页，选关页调用 `RequestAction(StartLevel, Index)`。条目可以继承普通 UserWidget，不必继承 SokobanScreen。

**Index 是 DA 目录中的原始索引**；排序或过滤之后也要使用保存的 Entry.Index，不能使用显示行号。CanStart 只代表目录身份检查通过，具体关卡布局仍在点击开始时由 Flow 校验；失败提示读取 Data.Status。空引用会保留索引，并将 CanStart=false。

进度在重新进入选关页时重新读取。若需要选关页内实时改列表，主动重新读取并更新条目；不要每帧重建整张列表。Completed 只表示当前修订的成绩存在，不表示解锁，所有合法关卡始终可选。

## 5. 按钮接线

使用普通 UMG Button，在 OnClicked 中调用当前页面的 `RequestAction`：

| 按钮用途 | Action | Index |
|---|---|---|
| 选择关卡／返回选关 | Select | 默认 -1 |
| 返回主菜单 | MainMenu | 默认 -1 |
| 某一关／最近关卡 | StartLevel | Entry.Index／LastPlayedIndex |
| 结算后重玩 | Replay | 默认 -1 |
| 下一关 | Next | 默认 -1 |
| 撤销／重做／重开 | Undo／Redo／Restart | 默认 -1 |
| 重试保存 | Save | 默认 -1 |
| 异常存档备份修复 | Repair | 默认 -1 |
| 退出 | Quit | 默认 -1 |

返回值代表请求是否被接受。动画期间、非法索引、存档失败等情况会返回 false；需要确认的 Repair / Quit 也先返回 false，并触发确认事件，不能把所有 false 都显示成“未知错误”。游戏操作仍由 GameMode 校验，禁用按钮不能代替后端边界检查。

旧 `Execute(Action, Index, Source)` 节点保留兼容，内部转发 RequestAction；Source 引脚不再修改按钮文字。旧的 SokobanActionButton 类型仅为保留已序列化引脚，新界面不用它。

## 6. 自定义确认框

实现 `Event On Confirmation Requested(Action, Reason)`，显示你自己设计的确认面板，并显示 Reason。确认按钮调用当前页面 `ResolveConfirmation(true)`；取消按钮调用 `ResolveConfirmation(false)`。根据返回值或 `ConfirmationPending=false` 关闭弹窗。

Repair 只有写保护时才请求确认；同意后由后端先备份原文件再恢复保存。Quit 会先尝试保存，仍有未保存状态才询问是否退出。没有实现该事件时，待确认操作会一直等待，不会自动修复或退出。

待确认期间，当前页面的新 RequestAction 被拒绝，重复点击不视为同意。请在菜单／结算页放置这些操作；弹窗的遮罩、焦点、按钮显隐等由蓝图负责。页面移除后确认失效，不能持有旧页面继续回调。事件中只展示弹窗，用户之后点击确认按钮再回调，不能在确认事件同步调用 ResolveConfirmation。

## 7. 输入与验收

- 菜单与结算使用 UI Only，游戏使用 Game and UI；页面切换清理方向按键状态。
- 游戏 HUD 的操作按钮建议关闭 **Is Focusable**，全屏根布局使用 **Not Hit-Testable (Self Only)**，避免空白区域截获鼠标；子按钮仍可点击。不要在游戏 HUD 中消费 WASD / Z / Y / R。
- 菜单若需要键盘导航，页面勾选 Is Focusable，并在蓝图 Construct 中设置默认按钮焦点；具体导航和视觉反馈由蓝图设计。
- 验证主菜单 → 选关 → 游戏 → 结算 → 下一关／返回；键盘移动后数字更新；点击撤销后仍能继续 WASD；动画期间按钮禁用；最后一关隐藏下一关；再次启动成绩保留。
- 自动化验证接口、布局不被 C++ 改写和 PIE 流程；测试临时 WBP 不写入 Content。正式按钮接线、视觉和焦点仍需完成 WBP 后人工试玩验收。
