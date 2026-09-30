#include "SokobanEditorDocument.h"

#include "Sokoban/Data/SokobanLevelData.h"
#include "Sokoban/Rules/SokobanLevelValidator.h"

FSokobanEditorSnapshot FSokobanEditorSnapshot::FromAsset(const USokobanLevelData& Asset)
{
	FSokobanEditorSnapshot Result;
	Result.LevelId = Asset.LevelId;
	Result.DisplayName = Asset.DisplayName;
	Result.ContentRevision = Asset.ContentRevision;
	Result.Definition = Asset.Definition;
	return Result;
}

void FSokobanEditorSnapshot::WriteTo(USokobanLevelData& Asset) const
{
	Asset.LevelId = LevelId;
	Asset.DisplayName = DisplayName;
	Asset.ContentRevision = ContentRevision;
	Asset.Definition = Definition;
}

bool FSokobanEditorSnapshot::Equals(const FSokobanEditorSnapshot& Other) const
{
	return LevelId == Other.LevelId && DisplayName.EqualTo(Other.DisplayName) && ContentRevision == Other.ContentRevision &&
		FSokobanLevelDefinition::StaticStruct()->CompareScriptStruct(&Definition, &Other.Definition, 0);
}

void FSokobanEditorDocument::NewLevel(int32 Width, int32 Height)
{
	Current = FSokobanEditorSnapshot();
	Current.DisplayName = FText::FromString(TEXT("新关卡"));
	Current.Definition.Width = FMath::Clamp(Width, 1, MaxDimension);
	Current.Definition.Height = FMath::Clamp(Height, 1, MaxDimension);
	Current.Definition.Terrain.Init(ESokobanTerrain::Floor, Current.Definition.Width * Current.Definition.Height);
	Saved = FSokobanEditorSnapshot();
	bHasSavedVersion = false;
	UndoHistory.Reset();
	RedoHistory.Reset();
}

void FSokobanEditorDocument::Load(const FSokobanEditorSnapshot& Snapshot)
{
	Current = Snapshot;
	Saved = Snapshot;
	bHasSavedVersion = true;
	UndoHistory.Reset();
	RedoHistory.Reset();
}

bool FSokobanEditorDocument::Commit(const FSokobanEditorSnapshot& Snapshot)
{
	if (Current.Equals(Snapshot)) { return false; }
	if (UndoHistory.Num() == MaxHistory) { UndoHistory.RemoveAt(0); }
	UndoHistory.Add(Current);
	Current = Snapshot;
	RedoHistory.Reset();
	return true;
}

bool FSokobanEditorDocument::Undo()
{
	if (!CanUndo()) { return false; }
	RedoHistory.Add(Current);
	Current = UndoHistory.Pop(EAllowShrinking::No);
	return true;
}

bool FSokobanEditorDocument::Redo()
{
	if (!CanRedo()) { return false; }
	UndoHistory.Add(Current);
	Current = RedoHistory.Pop(EAllowShrinking::No);
	return true;
}

bool FSokobanEditorDocument::CanDisplayGrid() const
{
	const auto& Level = Current.Definition;
	return Level.Width > 0 && Level.Width <= MaxDimension && Level.Height > 0 && Level.Height <= MaxDimension &&
		Level.Terrain.Num() == Level.Width * Level.Height;
}

bool FSokobanEditorDocument::IsInside(FIntPoint Cell) const
{
	return CanDisplayGrid() && Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Current.Definition.Width && Cell.Y < Current.Definition.Height;
}

ESokobanCellOccupant FSokobanEditorDocument::GetOccupant(FIntPoint Cell) const
{
	if (Current.Definition.PlayerSpawn == Cell) { return ESokobanCellOccupant::Player; }
	return Current.Definition.Boxes.ContainsByPredicate([Cell](const FSokobanBoxDefinition& Box) { return Box.Position == Cell; })
		? ESokobanCellOccupant::Box : ESokobanCellOccupant::None;
}

bool FSokobanEditorDocument::ApplyCell(FIntPoint Cell, ESokobanTerrain Terrain, bool bGoal, ESokobanCellOccupant Occupant)
{
	if (!IsInside(Cell) || (Terrain != ESokobanTerrain::Floor && Terrain != ESokobanTerrain::Wall && Terrain != ESokobanTerrain::Void) ||
		(Occupant != ESokobanCellOccupant::None && Occupant != ESokobanCellOccupant::Player && Occupant != ESokobanCellOccupant::Box)) { return false; }
	FSokobanEditorSnapshot Next = Current;
	FSokobanLevelDefinition& Level = Next.Definition;
	Level.Terrain[Cell.Y * Level.Width + Cell.X] = Terrain;
	// 墙和空洞不能保留目标或占用物；界面在提交前说明并确认这个清理动作。
	if (Terrain != ESokobanTerrain::Floor) { bGoal = false; Occupant = ESokobanCellOccupant::None; }
	if (bGoal) { Level.Goals.AddUnique(Cell); }
	else { Level.Goals.Remove(Cell); }
	int32 ExistingId = INDEX_NONE;
	const int32 ExistingIndex = Level.Boxes.IndexOfByPredicate([Cell](const FSokobanBoxDefinition& Box) { return Box.Position == Cell; });
	for (const auto& Box : Level.Boxes) { if (Box.Position == Cell && ExistingId == INDEX_NONE) { ExistingId = Box.BoxId; } }
	Level.Boxes.RemoveAll([Cell](const FSokobanBoxDefinition& Box) { return Box.Position == Cell; });
	if (Level.PlayerSpawn == Cell) { Level.PlayerSpawn = FIntPoint(-1, -1); }
	if (Occupant == ESokobanCellOccupant::Player)
	{
		// 单个坐标天然保证出生点唯一；放到新格子就是移动出生点。
		Level.PlayerSpawn = Cell;
	}
	else if (Occupant == ESokobanCellOccupant::Box)
	{
		TSet<int32> UsedIds;
		for (const auto& Box : Level.Boxes) { UsedIds.Add(Box.BoxId); }
		int32 Id = ExistingId;
		if (Id < 0 || UsedIds.Contains(Id))
		{
			Id = 0;
			while (UsedIds.Contains(Id)) { ++Id; }
		}
		FSokobanBoxDefinition Box;
		Box.BoxId = Id;
		Box.Position = Cell;
		if (ExistingIndex != INDEX_NONE) { Level.Boxes.Insert(Box, FMath::Min(ExistingIndex, Level.Boxes.Num())); }
		else { Level.Boxes.Add(Box); }
	}
	return Commit(Next);
}

bool FSokobanEditorDocument::Resize(int32 Width, int32 Height)
{
	if (Width < 1 || Height < 1 || Width > MaxDimension || Height > MaxDimension) { return false; }
	FSokobanEditorSnapshot Next = Current;
	auto& Level = Next.Definition;
	const auto& Old = Current.Definition;
	Level.Width = Width;
	Level.Height = Height;
	Level.Terrain.Init(ESokobanTerrain::Floor, Width * Height);
	for (int32 Y = 0; Y < FMath::Min(Height, Old.Height); ++Y)
	{
		for (int32 X = 0; X < FMath::Min(Width, Old.Width); ++X)
		{
			const int64 OldIndex = static_cast<int64>(Y) * Old.Width + X;
			if (OldIndex >= 0 && OldIndex < Old.Terrain.Num()) { Level.Terrain[Y * Width + X] = Old.Terrain[static_cast<int32>(OldIndex)]; }
		}
	}
	const auto Inside = [Width, Height](FIntPoint Cell) { return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Width && Cell.Y < Height; };
	Level.Goals.RemoveAll([&Inside](FIntPoint Cell) { return !Inside(Cell); });
	Level.Boxes.RemoveAll([&Inside](const FSokobanBoxDefinition& Box) { return !Inside(Box.Position); });
	if (!Inside(Level.PlayerSpawn)) { Level.PlayerSpawn = FIntPoint(-1, -1); }
	return Commit(Next);
}

TArray<FSokobanValidationIssue> FSokobanEditorDocument::Validate() const
{
	TArray<FSokobanValidationIssue> Issues = FSokobanLevelValidator::Validate(Current.Definition);
	if (Current.LevelId.IsNone())
	{
		auto& Issue = Issues.AddDefaulted_GetRef();
		Issue.Code = TEXT("MissingLevelId");
		Issue.Message = FText::FromString(TEXT("请填写关卡 ID；首次保存时可以使用资产名称作为 ID。"));
	}
	return Issues;
}
