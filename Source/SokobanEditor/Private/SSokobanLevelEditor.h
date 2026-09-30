#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "SokobanEditorDocument.h"
#include "Sokoban/Data/SokobanLevelData.h"

class SVerticalBox;
class STextComboBox;
struct FAssetData;

/** 三栏关卡配置面板：关卡信息、可点击网格、选中格子的属性。没有拖动绘制行为。 */
class SSokobanLevelEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSokobanLevelEditor) : _InitialAsset(nullptr) {}
		SLATE_ARGUMENT(USokobanLevelData*, InitialAsset)
	SLATE_END_ARGS()
	void Construct(const FArguments& Arguments);
	bool CanClose();
	void OpenAsset(USokobanLevelData* Asset);

private:
	TSharedRef<SWidget> MakeLevelPanel();
	TSharedRef<SWidget> MakeCellPanel();
	void Refresh(bool bRebuildGrid = false);
	void RebuildGrid();
	void RefreshIssues();
	void ReadSelectedCell();
	void SelectCell(FIntPoint Cell);
	bool HasPendingCell() const;
	bool ResolvePendingCell();
	bool ApplySelectedCell();
	bool ConfirmLeaveDocument();
	bool Save();
	bool FindDuplicateId(FName Id, FString& OutAssetPath) const;
	FReply NewLevel();
	FReply ResizeLevel();
	FReply Undo();
	FReply Redo();
	FText CellLabel(FIntPoint Cell) const;
	FSlateColor CellColor(FIntPoint Cell) const;
	void SetStatus(const FString& Text);

	FSokobanEditorDocument Document;
	TStrongObjectPtr<USokobanLevelData> EditingAsset;
	TSharedPtr<SVerticalBox> GridHost;
	TSharedPtr<SVerticalBox> IssueHost;
	TSharedPtr<STextComboBox> TerrainCombo;
	TSharedPtr<STextComboBox> OccupantCombo;
	TArray<TSharedPtr<FString>> TerrainOptions;
	TArray<TSharedPtr<FString>> OccupantOptions;
	TSet<FIntPoint> IssueCells;
	FIntPoint SelectedCell = FIntPoint(0, 0);
	int32 ProposedWidth = 7;
	int32 ProposedHeight = 5;
	ESokobanTerrain PendingTerrain = ESokobanTerrain::Floor;
	ESokobanCellOccupant PendingOccupant = ESokobanCellOccupant::None;
	bool bPendingGoal = false;
	bool bReadingCell = false;
	FText Status;
};
