#include "SSokobanLevelEditor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "Factories/DataAssetFactory.h"
#include "FileHelpers.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "PropertyCustomizationHelpers.h"
#include "Sokoban/Data/SokobanLevelData.h"
#include "Styling/AppStyle.h"
#include "Templates/UnrealTemplate.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FText Text(const TCHAR* Value) { return FText::FromString(Value); }
	TSharedRef<STextBlock> Label(const TCHAR* Value) { return SNew(STextBlock).Text(Text(Value)).AutoWrapText(true); }
	const TCHAR* TerrainName(ESokobanTerrain Terrain)
	{
		return Terrain == ESokobanTerrain::Floor ? TEXT("地板") : Terrain == ESokobanTerrain::Wall ? TEXT("墙") : TEXT("空洞");
	}
}

void SSokobanLevelEditor::Construct(const FArguments& Arguments)
{
	Document.NewLevel();
	if (Arguments._InitialAsset && Arguments._InitialAsset->DataVersion == 1)
	{
		EditingAsset.Reset(Arguments._InitialAsset);
		Document.Load(FSokobanEditorSnapshot::FromAsset(*Arguments._InitialAsset));
	}
	TerrainOptions = {MakeShared<FString>(TEXT("地板")), MakeShared<FString>(TEXT("墙")), MakeShared<FString>(TEXT("空洞"))};
	OccupantOptions = {MakeShared<FString>(TEXT("无")), MakeShared<FString>(TEXT("玩家出生点")), MakeShared<FString>(TEXT("箱子"))};
	ChildSlot
	[
		SNew(SBorder).Padding(10).IsEnabled_Lambda([] { return !GEditor || !GEditor->PlayWorld; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(3)[SNew(SButton).Text(Text(TEXT("新建关卡"))).OnClicked(this, &SSokobanLevelEditor::NewLevel)]
				+ SHorizontalBox::Slot().FillWidth(1).Padding(3)
				[
					SNew(SObjectPropertyEntryBox).AllowedClass(USokobanLevelData::StaticClass()).AllowClear(false)
					.ObjectPath_Lambda([this] { return EditingAsset.IsValid() ? EditingAsset->GetPathName() : FString(); })
					.OnObjectChanged_Lambda([this](const FAssetData& Asset) { OpenAsset(Cast<USokobanLevelData>(Asset.GetAsset())); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3)[SNew(SButton).Text(Text(TEXT("保存资产"))).OnClicked_Lambda([this] { Save(); return FReply::Handled(); })]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3)[SNew(SButton).Text(Text(TEXT("撤销编辑"))).IsEnabled_Lambda([this] { return Document.CanUndo() || HasPendingCell(); }).OnClicked(this, &SSokobanLevelEditor::Undo)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3)[SNew(SButton).Text(Text(TEXT("重做编辑"))).IsEnabled_Lambda([this] { return Document.CanRedo() && !HasPendingCell(); }).OnClicked(this, &SSokobanLevelEditor::Redo)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3)[SNew(SButton).Text(Text(TEXT("校验关卡"))).OnClicked_Lambda([this] { if (ResolvePendingCell()) { RefreshIssues(); SetStatus(TEXT("校验已刷新；数据合法不代表谜题一定可解。")); } return FReply::Handled(); })]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(3)
			[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(FString::Printf(TEXT("%s%s"), EditingAsset.IsValid() ? *EditingAsset->GetPathName() : TEXT("新关卡草稿"), Document.IsDirty() || HasPendingCell() ? TEXT("  * 未保存") : TEXT("  已保存"))); })]
			+ SVerticalBox::Slot().FillHeight(1)
			[
				SNew(SSplitter)
				+ SSplitter::Slot().Value(0.23f)[MakeLevelPanel()]
				+ SSplitter::Slot().Value(0.52f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(6)[Label(TEXT("点击格子进行配置。左上角为 (0,0)，X 向右、Y 向下。"))]
					+ SVerticalBox::Slot().FillHeight(1)
					[
						SNew(SScrollBox).Orientation(Orient_Horizontal)
						+ SScrollBox::Slot()[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(GridHost, SVerticalBox)]]
					]
				]
				+ SSplitter::Slot().Value(0.25f)[MakeCellPanel()]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Status; })]
		]
	];
	Refresh(true);
	SetStatus(TEXT("选择一个格子，在右侧配置内容后点击“应用到格子”。工具内保留最近 128 次编辑历史。"));
}

TSharedRef<SWidget> SSokobanLevelEditor::MakeLevelPanel()
{
	return SNew(SBorder).Padding(8)
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)[Label(TEXT("关卡 ID（项目内唯一）"))]
			+ SVerticalBox::Slot().AutoHeight()
			[SNew(SEditableTextBox).Text_Lambda([this] { return Document.GetData().LevelId.IsNone() ? FText::GetEmpty() : FText::FromName(Document.GetData().LevelId); })
			.OnTextCommitted_Lambda([this](const FText& Value, ETextCommit::Type) { auto Next = Document.GetData(); Next.LevelId = FName(*Value.ToString().TrimStartAndEnd()); Document.Commit(Next); RefreshIssues(); })]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 5)[Label(TEXT("关卡名称"))]
			+ SVerticalBox::Slot().AutoHeight()
			[SNew(SEditableTextBox).Text_Lambda([this] { return Document.GetData().DisplayName; })
			.OnTextCommitted_Lambda([this](const FText& Value, ETextCommit::Type) { auto Next = Document.GetData(); Next.DisplayName = Value; Document.Commit(Next); })]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(FString::Printf(TEXT("内容修订号：%d（保存布局变更时递增）"), Document.GetData().ContentRevision)); }).AutoWrapText(true)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)[Label(TEXT("地图尺寸（1～64，点击调整后生效）"))]
			+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("宽度 X"))]
			+ SVerticalBox::Slot().AutoHeight()[SNew(SSpinBox<int32>).MinValue(1).MaxValue(FSokobanEditorDocument::MaxDimension).Value_Lambda([this] { return ProposedWidth; }).OnValueChanged_Lambda([this](int32 Value) { ProposedWidth = Value; })]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)[Label(TEXT("高度 Y"))]
			+ SVerticalBox::Slot().AutoHeight()[SNew(SSpinBox<int32>).MinValue(1).MaxValue(FSokobanEditorDocument::MaxDimension).Value_Lambda([this] { return ProposedHeight; }).OnValueChanged_Lambda([this](int32 Value) { ProposedHeight = Value; })]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)[SNew(SButton).Text(Text(TEXT("调整地图尺寸"))).OnClicked(this, &SSokobanLevelEditor::ResizeLevel)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)[Label(TEXT("校验结果（点击问题定位格子）"))]
			+ SVerticalBox::Slot().AutoHeight()[SAssignNew(IssueHost, SVerticalBox)]
		]
	];
}

TSharedRef<SWidget> SSokobanLevelEditor::MakeCellPanel()
{
	return SNew(SBorder).Padding(8)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)
		[SNew(STextBlock).Text_Lambda([this] { return Document.IsInside(SelectedCell) ? FText::FromString(FString::Printf(TEXT("选中格子：(%d, %d)"), SelectedCell.X, SelectedCell.Y)) : Text(TEXT("请先调整尺寸生成网格")); })]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)[Label(TEXT("地形"))]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SAssignNew(TerrainCombo, STextComboBox).OptionsSource(&TerrainOptions).IsEnabled_Lambda([this] { return Document.IsInside(SelectedCell); })
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Choice, ESelectInfo::Type)
			{
				if (bReadingCell) { return; }
				const int32 Index = TerrainOptions.IndexOfByKey(Choice);
				PendingTerrain = Index == 0 ? ESokobanTerrain::Floor : Index == 1 ? ESokobanTerrain::Wall : ESokobanTerrain::Void;
				if (PendingTerrain != ESokobanTerrain::Floor) { bPendingGoal = false; PendingOccupant = ESokobanCellOccupant::None; OccupantCombo->SetSelectedItem(OccupantOptions[0]); }
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12)
		[SNew(SCheckBox).IsEnabled_Lambda([this] { return Document.IsInside(SelectedCell) && PendingTerrain == ESokobanTerrain::Floor; })
		.IsChecked_Lambda([this] { return bPendingGoal ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bPendingGoal = State == ECheckBoxState::Checked; })[Label(TEXT("这个格子是目标点"))]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)[Label(TEXT("占用物"))]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SAssignNew(OccupantCombo, STextComboBox).OptionsSource(&OccupantOptions)
			.IsEnabled_Lambda([this] { return Document.IsInside(SelectedCell) && PendingTerrain == ESokobanTerrain::Floor; })
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Choice, ESelectInfo::Type) { if (!bReadingCell) { PendingOccupant = static_cast<ESokobanCellOccupant>(FMath::Max(0, OccupantOptions.IndexOfByKey(Choice))); } })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 14)
		[SNew(SButton).Text(Text(TEXT("应用到格子"))).IsEnabled_Lambda([this] { return Document.IsInside(SelectedCell) && HasPendingCell(); })
		.OnClicked_Lambda([this] { ApplySelectedCell(); return FReply::Handled(); })]
		+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("占用物选“无”可移除箱子或出生点。目标点可与玩家或箱子重叠。放置玩家会移动唯一的出生点。"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 14)[Label(TEXT("设为墙或空洞将清除本格的目标和占用物。黄色为目标，橙色为箱子，绿色为目标上的箱子，蓝色为玩家；青色边框表示选中，红色边框表示校验问题。"))]
	];
}

void SSokobanLevelEditor::RebuildGrid()
{
	GridHost->ClearChildren();
	if (!Document.CanDisplayGrid())
	{
		GridHost->AddSlot().AutoHeight().Padding(12)[Label(TEXT("当前尺寸或地形数组无法显示。请在左侧设置 1～64 的宽高，再点击“调整地图尺寸”；此操作可撤销。"))];
		return;
	}
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(2);
	const auto& Level = Document.GetData().Definition;
	for (int32 Y = 0; Y < Level.Height; ++Y)
	{
		for (int32 X = 0; X < Level.Width; ++X)
		{
			const FIntPoint Cell(X, Y);
			Grid->AddSlot(X, Y)
			[
				SNew(SBox).WidthOverride(64).HeightOverride(58)
				[
					SNew(SBorder).Padding(2).BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor_Lambda([this, Cell] { return SelectedCell == Cell ? FLinearColor(0.f, 0.8f, 1.f) : IssueCells.Contains(Cell) ? FLinearColor(1.f, 0.1f, 0.1f) : FLinearColor::Transparent; })
					[
						SNew(SButton).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.ButtonColorAndOpacity_Lambda([this, Cell] { return CellColor(Cell); })
						.OnClicked_Lambda([this, Cell] { SelectCell(Cell); return FReply::Handled(); })
						[SNew(STextBlock).Justification(ETextJustify::Center).Text_Lambda([this, Cell] { return CellLabel(Cell); })]
					]
				]
			];
		}
	}
	GridHost->AddSlot().AutoHeight()[Grid];
}

FText SSokobanLevelEditor::CellLabel(FIntPoint Cell) const
{
	if (!Document.IsInside(Cell)) { return FText::GetEmpty(); }
	const auto& Level = Document.GetData().Definition;
	FString Name = TerrainName(Level.Terrain[Cell.Y * Level.Width + Cell.X]);
	const auto Occupant = Document.GetOccupant(Cell);
	if (Occupant == ESokobanCellOccupant::Box) { Name = TEXT("箱子"); }
	if (Occupant == ESokobanCellOccupant::Player) { Name = TEXT("玩家"); }
	if (Level.Goals.Contains(Cell)) { Name = Occupant == ESokobanCellOccupant::None ? TEXT("目标") : Name + TEXT("·目标"); }
	return FText::FromString(FString::Printf(TEXT("%s\n%d,%d"), *Name, Cell.X, Cell.Y));
}

FSlateColor SSokobanLevelEditor::CellColor(FIntPoint Cell) const
{
	if (!Document.IsInside(Cell)) { return FLinearColor::Black; }
	const auto& Level = Document.GetData().Definition;
	const auto Terrain = Level.Terrain[Cell.Y * Level.Width + Cell.X];
	if (Terrain == ESokobanTerrain::Void) { return FLinearColor(0.015f, 0.015f, 0.015f); }
	if (Terrain == ESokobanTerrain::Wall) { return FLinearColor(0.06f, 0.08f, 0.12f); }
	const bool bGoal = Level.Goals.Contains(Cell);
	switch (Document.GetOccupant(Cell))
	{
	case ESokobanCellOccupant::Player: return FLinearColor(0.05f, 0.3f, 0.75f);
	case ESokobanCellOccupant::Box: return bGoal ? FLinearColor(0.04f, 0.5f, 0.13f) : FLinearColor(0.65f, 0.22f, 0.02f);
	default: return bGoal ? FLinearColor(0.65f, 0.45f, 0.02f) : FLinearColor(0.22f, 0.25f, 0.3f);
	}
}

void SSokobanLevelEditor::Refresh(bool bRebuildGrid)
{
	if (!Document.IsInside(SelectedCell)) { SelectedCell = FIntPoint(0, 0); }
	ProposedWidth = FMath::Clamp(Document.GetData().Definition.Width, 1, FSokobanEditorDocument::MaxDimension);
	ProposedHeight = FMath::Clamp(Document.GetData().Definition.Height, 1, FSokobanEditorDocument::MaxDimension);
	if (bRebuildGrid) { RebuildGrid(); }
	ReadSelectedCell();
	RefreshIssues();
}

void SSokobanLevelEditor::ReadSelectedCell()
{
	TGuardValue<bool> Guard(bReadingCell, true);
	const auto& Level = Document.GetData().Definition;
	PendingTerrain = Document.IsInside(SelectedCell) ? Level.Terrain[SelectedCell.Y * Level.Width + SelectedCell.X] : ESokobanTerrain::Floor;
	bPendingGoal = Level.Goals.Contains(SelectedCell);
	PendingOccupant = Document.GetOccupant(SelectedCell);
	TerrainCombo->SetSelectedItem(TerrainOptions[PendingTerrain == ESokobanTerrain::Floor ? 0 : PendingTerrain == ESokobanTerrain::Wall ? 1 : 2]);
	OccupantCombo->SetSelectedItem(OccupantOptions[static_cast<int32>(PendingOccupant)]);
}

bool SSokobanLevelEditor::HasPendingCell() const
{
	if (!Document.IsInside(SelectedCell)) { return false; }
	const auto& Level = Document.GetData().Definition;
	return PendingTerrain != Level.Terrain[SelectedCell.Y * Level.Width + SelectedCell.X] ||
		bPendingGoal != Level.Goals.Contains(SelectedCell) || PendingOccupant != Document.GetOccupant(SelectedCell);
}

bool SSokobanLevelEditor::ApplySelectedCell()
{
	if (!Document.IsInside(SelectedCell)) { return false; }
	const auto& Level = Document.GetData().Definition;
	if (PendingTerrain != ESokobanTerrain::Floor && (Level.Goals.Contains(SelectedCell) || Document.GetOccupant(SelectedCell) != ESokobanCellOccupant::None))
	{
		if (FMessageDialog::Open(EAppMsgType::YesNo, Text(TEXT("改成墙或空洞会清除这个格子的目标点、箱子或玩家出生点。是否应用？可用“撤销编辑”恢复。"))) != EAppReturnType::Yes) { return false; }
	}
	Document.ApplyCell(SelectedCell, PendingTerrain, bPendingGoal, PendingOccupant);
	ReadSelectedCell();
	RefreshIssues();
	SetStatus(TEXT("格子已更新到草稿。点击“保存资产”写回关卡。"));
	return true;
}

bool SSokobanLevelEditor::ResolvePendingCell()
{
	if (!HasPendingCell()) { return true; }
	const auto Reply = FMessageDialog::Open(EAppMsgType::YesNoCancel, Text(TEXT("当前格子有尚未应用的设置。是否先应用？\n是：应用；否：放弃本格设置；取消：继续编辑。")));
	if (Reply == EAppReturnType::Cancel) { return false; }
	if (Reply == EAppReturnType::Yes) { return ApplySelectedCell(); }
	ReadSelectedCell();
	return true;
}

void SSokobanLevelEditor::SelectCell(FIntPoint Cell)
{
	if (Cell == SelectedCell || !Document.IsInside(Cell) || !ResolvePendingCell()) { return; }
	SelectedCell = Cell;
	ReadSelectedCell();
}

bool SSokobanLevelEditor::FindDuplicateId(FName Id, FString& OutAssetPath) const
{
	if (Id.IsNone()) { return false; }
	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByClass(USokobanLevelData::StaticClass()->GetClassPathName(), Assets, true);
	for (const auto& Entry : Assets)
	{
		// 读取实际对象，以包含另一个属性窗口中尚未保存的 ID 修改。
		const auto* Other = Cast<USokobanLevelData>(Entry.GetAsset());
		if (Other && Other != EditingAsset.Get() && Other->LevelId == Id) { OutAssetPath = Other->GetPathName(); return true; }
	}
	return false;
}

void SSokobanLevelEditor::RefreshIssues()
{
	IssueHost->ClearChildren();
	IssueCells.Reset();
	auto Issues = Document.Validate();
	FString DuplicatePath;
	if (FindDuplicateId(Document.GetData().LevelId, DuplicatePath))
	{
		auto& Issue = Issues.AddDefaulted_GetRef();
		Issue.Message = FText::FromString(TEXT("关卡 ID 已被使用：") + DuplicatePath);
	}
	if (Issues.IsEmpty()) { IssueHost->AddSlot().AutoHeight()[Label(TEXT("结构校验通过。是否可解仍需试玩确认。"))]; }
	for (const auto& Issue : Issues)
	{
		FIntPoint Focus(-1, -1);
		for (FIntPoint Cell : Issue.Cells) { if (Document.IsInside(Cell)) { IssueCells.Add(Cell); if (Focus.X < 0) { Focus = Cell; } } }
		IssueHost->AddSlot().AutoHeight().Padding(0, 3)
		[SNew(SButton).OnClicked_Lambda([this, Focus] { if (Document.IsInside(Focus)) { SelectCell(Focus); } return FReply::Handled(); })
		[SNew(STextBlock).Text(Issue.Message).AutoWrapText(true)]];
	}
}

void SSokobanLevelEditor::SetStatus(const FString& Value) { Status = FText::FromString(Value); }

bool SSokobanLevelEditor::ConfirmLeaveDocument()
{
	if (!ResolvePendingCell()) { return false; }
	if (!Document.IsDirty()) { return true; }
	const auto Reply = FMessageDialog::Open(EAppMsgType::YesNoCancel, Text(TEXT("关卡草稿尚未保存。是否保存？\n是：保存；否：放弃草稿修改；取消：留在当前关卡。")));
	return Reply == EAppReturnType::Yes ? Save() : Reply == EAppReturnType::No;
}

bool SSokobanLevelEditor::CanClose() { return ConfirmLeaveDocument(); }

void SSokobanLevelEditor::OpenAsset(USokobanLevelData* Asset)
{
	if (!Asset || Asset == EditingAsset.Get()) { return; }
	if (Asset->DataVersion != 1) { SetStatus(TEXT("此工具只支持数据格式版本 1，未打开或改写该资产。")); return; }
	if (!ConfirmLeaveDocument()) { return; }
	EditingAsset.Reset(Asset);
	Document.Load(FSokobanEditorSnapshot::FromAsset(*Asset));
	Refresh(true);
	SetStatus(TEXT("关卡已载入草稿，原资产在保存前不会被修改。"));
}

FReply SSokobanLevelEditor::NewLevel()
{
	if (ConfirmLeaveDocument()) { EditingAsset.Reset(); Document.NewLevel(); SelectedCell = FIntPoint(0, 0); Refresh(true); SetStatus(TEXT("已新建 7×5 地板草稿，请设置出生点、箱子和目标。")); }
	return FReply::Handled();
}

FReply SSokobanLevelEditor::ResizeLevel()
{
	if (!ResolvePendingCell()) { return FReply::Handled(); }
	const auto& Level = Document.GetData().Definition;
	if ((ProposedWidth < Level.Width || ProposedHeight < Level.Height || !Document.CanDisplayGrid()) &&
		FMessageDialog::Open(EAppMsgType::YesNo, Text(TEXT("调整尺寸会保留左上角重叠区域，并移除范围外的箱子、目标和出生点。地形数组缺少的格子将补为地板。是否继续？此操作可撤销。"))) != EAppReturnType::Yes) { return FReply::Handled(); }
	Document.Resize(ProposedWidth, ProposedHeight);
	Refresh(true);
	return FReply::Handled();
}

FReply SSokobanLevelEditor::Undo()
{
	// 尚未应用的格子属性优先撤回；不影响全局 UE 场景操作历史。
	if (HasPendingCell()) { ReadSelectedCell(); }
	else if (Document.Undo()) { Refresh(true); }
	return FReply::Handled();
}

FReply SSokobanLevelEditor::Redo()
{
	if (!HasPendingCell() && Document.Redo()) { Refresh(true); }
	return FReply::Handled();
}

bool SSokobanLevelEditor::Save()
{
	if (!ResolvePendingCell()) { return false; }
	FSokobanEditorSnapshot ToSave = Document.GetData();
	FString NewPackage;
	if (!EditingAsset.IsValid())
	{
		FSaveAssetDialogConfig Config;
		Config.DialogTitleOverride = Text(TEXT("保存推箱子关卡"));
		Config.DefaultPath = TEXT("/Game/Sokoban/Levels");
		Config.DefaultAssetName = TEXT("DA_Level01");
		Config.AssetClassNames.Add(USokobanLevelData::StaticClass()->GetClassPathName());
		Config.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;
		NewPackage = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().CreateModalSaveAssetDialog(Config);
		if (NewPackage.IsEmpty()) { return false; }
		NewPackage = FPackageName::ObjectPathToPackageName(NewPackage);
		if (ToSave.LevelId.IsNone()) { ToSave.LevelId = FName(*FPackageName::GetLongPackageAssetName(NewPackage)); }
	}
	if (ToSave.LevelId.IsNone()) { SetStatus(TEXT("请先填写关卡 ID，再保存。")); return false; }
	FString DuplicatePath;
	if (FindDuplicateId(ToSave.LevelId, DuplicatePath)) { SetStatus(TEXT("保存失败：关卡 ID 已被使用：") + DuplicatePath); return false; }
	if (EditingAsset.IsValid())
	{
		const auto CurrentAsset = FSokobanEditorSnapshot::FromAsset(*EditingAsset.Get());
		if (!CurrentAsset.Equals(Document.GetSavedData()) && FMessageDialog::Open(EAppMsgType::YesNo,
			Text(TEXT("原资产在工具外被修改过。是否用当前草稿覆盖这些修改？选择“否”可以保留两边内容，稍后处理。"))) != EAppReturnType::Yes) { return false; }
		if (!FSokobanLevelDefinition::StaticStruct()->CompareScriptStruct(&ToSave.Definition, &CurrentAsset.Definition, 0))
		{
			if (CurrentAsset.ContentRevision == MAX_int32) { SetStatus(TEXT("内容修订号已到上限，无法继续保存布局修改。")); return false; }
			ToSave.ContentRevision = FMath::Max(ToSave.ContentRevision, CurrentAsset.ContentRevision + 1);
		}
	}
	else
	{
		UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
		Factory->DataAssetClass = USokobanLevelData::StaticClass();
		auto& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		EditingAsset.Reset(Cast<USokobanLevelData>(Tools.CreateAsset(FPackageName::GetLongPackageAssetName(NewPackage), FPackageName::GetLongPackagePath(NewPackage), USokobanLevelData::StaticClass(), Factory)));
		if (!EditingAsset.IsValid()) { SetStatus(TEXT("创建资产失败，请检查路径和文件权限。")); return false; }
	}
	// 草稿允许未完成；结构问题在面板中可见，不能据此误认为已经可以试玩。
	const auto Before = FSokobanEditorSnapshot::FromAsset(*EditingAsset.Get());
	ToSave.WriteTo(*EditingAsset.Get());
	EditingAsset->MarkPackageDirty();
	EditingAsset->PostEditChange();
	const TArray<UPackage*> Packages = {EditingAsset->GetOutermost()};
	const auto Result = FEditorFileUtils::PromptForCheckoutAndSave(Packages, false, false);
	if (Result != FEditorFileUtils::PR_Success)
	{
		Before.WriteTo(*EditingAsset.Get());
		EditingAsset->PostEditChange();
		SetStatus(TEXT("保存未完成，草稿仍保留在工具中，可以重试。"));
		return false;
	}
	Document.AcceptSaved(ToSave);
	RefreshIssues();
	SetStatus(Document.Validate().IsEmpty() ? TEXT("资产已保存，结构校验通过。请在 GameMode 的 Startup Level 中指定它进行试玩。") : TEXT("草稿已保存，但仍有校验问题；请按左侧提示完善后再试玩。"));
	return true;
}
