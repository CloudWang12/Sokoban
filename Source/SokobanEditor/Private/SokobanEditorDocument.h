#pragma once

#include "CoreMinimal.h"
#include "Sokoban/Data/SokobanTypes.h"

class USokobanLevelData;

/** 仅供编辑工具使用。目标点是独立标记，不占用这个枚举的一个选项。 */
enum class ESokobanCellOccupant : uint8 { None, Player, Box };

/** 编辑草稿的一份快照；历史只属于当前工具，不会撤销场景中其他 Actor 的操作。 */
struct FSokobanEditorSnapshot
{
	FName LevelId;
	FText DisplayName;
	int32 ContentRevision = 1;
	FSokobanLevelDefinition Definition;
	static FSokobanEditorSnapshot FromAsset(const USokobanLevelData& Asset);
	void WriteTo(USokobanLevelData& Asset) const;
	bool Equals(const FSokobanEditorSnapshot& Other) const;
};

/** 无场景依赖的编辑模型：在副本上配置格子，保存前不修改原始关卡资产。 */
class FSokobanEditorDocument
{
public:
	static constexpr int32 MaxDimension = 64;
	static constexpr int32 MaxHistory = 128;
	void NewLevel(int32 Width = 7, int32 Height = 5);
	void Load(const FSokobanEditorSnapshot& Snapshot);
	const FSokobanEditorSnapshot& GetData() const { return Current; }
	const FSokobanEditorSnapshot& GetSavedData() const { return Saved; }
	bool IsDirty() const { return !bHasSavedVersion || !Current.Equals(Saved); }
	void MarkSaved() { Saved = Current; bHasSavedVersion = true; }
	void AcceptSaved(const FSokobanEditorSnapshot& Snapshot) { Current = Snapshot; MarkSaved(); }
	bool Commit(const FSokobanEditorSnapshot& Snapshot);
	bool CanUndo() const { return !UndoHistory.IsEmpty(); }
	bool CanRedo() const { return !RedoHistory.IsEmpty(); }
	bool Undo();
	bool Redo();
	bool CanDisplayGrid() const;
	bool IsInside(FIntPoint Cell) const;
	ESokobanCellOccupant GetOccupant(FIntPoint Cell) const;
	bool ApplyCell(FIntPoint Cell, ESokobanTerrain Terrain, bool bGoal, ESokobanCellOccupant Occupant);
	/** 从左上角保留重叠区域，新格子为地板；裁剪范围外的目标、箱子和出生点。 */
	bool Resize(int32 Width, int32 Height);
	TArray<FSokobanValidationIssue> Validate() const;

private:
	FSokobanEditorSnapshot Current;
	FSokobanEditorSnapshot Saved;
	TArray<FSokobanEditorSnapshot> UndoHistory;
	TArray<FSokobanEditorSnapshot> RedoHistory;
	bool bHasSavedVersion = false;
};
