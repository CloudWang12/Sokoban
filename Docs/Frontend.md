# 主菜单、选关、存档与结算接入

当前 C++ 流程支持：主菜单 → 选择关卡 → 游戏 → 正式结算 → 下一关／重玩／返回选关。所有关卡始终开放，不做解锁。运行界面由 Widget Blueprint 制作，C++ 不提供默认布局。

## 1. 试玩前配置

打开 `/Game/Sokoban/L_SokobanPlayerGround`。`BP_SokobanGameMode` 中 `Enable Frontend` 开启，`Level Catalog` 指向 `/Game/Sokoban/Levels/DA_SokobanCatalog`。先按 [UMG 蓝图接入](BlueprintUI.md) 完成四个 WBP 的布局、事件和控制器 UIManager 配置，再 Play。

目录包含两关：

| DA | ID | 可复现解法 |
|---|---|---|
| DA_Level_001 | Level_001 | W、S、D、D、W；5 步、2 推 |
| DA_Level_002 | Level_002 | D、D、W；3 步、1 推 |

每次输入等动画结束，相同按键需分别按下。WASD／方向键移动、Z 撤销、Y 重做、R 重开。正式结算后不能撤销该次成绩，需要 Replay 开始新局。

`Startup Level` 与 `Use Demo Level If Unset` 仅供关闭 `Enable Frontend` 后的旧单关调试使用。正式模式以目录为入口，不回退到内置示例或 C++ 默认界面。

## 2. 新关卡如何加入

1. 用关卡编辑器制作、校验并保存 `USokobanLevelData`，设置项目内唯一 LevelId。
2. 在 `DA_SokobanCatalog.Levels` 加入 DA。数组顺序决定原始选关索引和下一关。
3. 保存目录，重新 Play。蓝图选关列表从 GetLevelEntries 获取条目。
4. 目录不能有空引用、重复 ID、未知 DataVersion 或非法修订号；开始时再次校验布局。蓝图应显示 Data.Status 提供的失败原因。

目录对关卡使用硬引用，GameMode 蓝图引用目录，因此这些 DA 能通过入口依赖参与烘焙。改用软引用时需要重新检查烘焙收集。关卡编辑器允许保存草稿，不代表草稿可直接交付试玩。

## 3. 存档规则

- 默认槽位 `SokobanProgress`，开发环境通常位于 Saved/SaveGames；不提交玩家存档。
- 保存 LevelId、ContentRevision、最佳成绩与最近关卡。最近关卡从起点重玩，不恢复中途棋盘或撤销栈。
- 最佳成绩按步数优先，步数相同时比较推动数，保留同一次解法的完整成绩。
- 修订不同的旧成绩不展示、不比较；再次通关后替换该 ID 的旧修订成绩，不保存多版本成绩档案。
- 读取失败、未知版本、非法成绩均启用写保护，保留原文件。仍可在内存记录本次成绩。
- Save 命令重试保存。Repair 命令先通知蓝图显示确认框，用户确认后调用 ResolveConfirmation(true)，成功备份原始字节后才恢复写入。备份名带唯一后缀，不自动删除。
- 保存失败不阻止结算。Quit 先尝试保存，仍有未保存状态时通过同一蓝图确认机制处理。崩溃、强制退出不保证保留内存进度。

## 4. 接口职责

| 类型 | 职责 |
|---|---|
| USokobanLevelCatalog | 目录、ID 查找、下一关、目录校验 |
| USokobanFlowSubsystem | 四种流程状态、开始／重玩／换关／返回、单次结算 |
| USokobanSaveGame / FSokobanLevelProgress | 序列化、成绩比较、版本验证 |
| USokobanProgressSubsystem | 读盘、保存、失败状态、备份恢复 |
| USokobanUIManagerComponent | 创建配置的 WBP、页面切换、输入模式与销毁 |
| USokobanScreen / 四个派生父类 | GetViewData、GetLevelEntries、数据变更事件、RequestAction、确认接口 |
| 四个 WBP | 布局、样式、控件、数据显示、按钮事件、确认框和动画 |

UI 不直接改 Session 或 DA。游戏命令经 GameMode，菜单／结算禁止移动；页面切换清理方向状态。同一页面的数据通知不会重建 WBP。Flow 弱引用 GameMode，世界销毁时解绑；控制器销毁时移除界面和监听。

## 5. 验收

检查最后一步动画完成后只结算一次，Z／Y／R 不改变已结算棋盘；下一关清空历史和计数，最后一关的 HasNextLevel=false；重启后保留成绩，改变修订后不显示旧成绩。

蓝图需展示空目录、重复 ID、布局错误与存档失败状态，并接入确认框。验证点击 HUD 按钮后仍能使用移动快捷键。自动化使用专用随机存档槽，禁止破坏正式玩家进度。

`Sokoban.Frontend` 6 项测试覆盖目录、成绩版本、磁盘读写、流程结算、WBP 布局归属与数据／确认接口、真实地图 PIE 页面切换。UI 测试使用临时 WBP，不修改用户资产；不再生成默认 UI 截图。它不等同于用户 WBP 按钮、视觉验收或 Shipping 打包测试。
