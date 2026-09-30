#include "Modules/ModuleManager.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/Docking/TabManager.h"
#include "SSokobanLevelEditor.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

namespace { const FName SokobanTabId(TEXT("SokobanLevelEditor")); }

/** 此模块只在编辑器目标加载，运行时 Sokoban 模块不反向依赖工具代码。 */
class FSokobanEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet()) { return; }
		bRegistered = true;
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SokobanTabId, FOnSpawnTab::CreateRaw(this, &FSokobanEditorModule::SpawnTab))
			.SetDisplayName(FText::FromString(TEXT("推箱子关卡编辑器"))).SetMenuType(ETabSpawnerMenuType::Hidden);
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSokobanEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		if (!bRegistered) { return; }
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SokobanTabId);
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
		Menu->FindOrAddSection(TEXT("Sokoban")).AddMenuEntry(TEXT("OpenSokobanLevelEditor"),
			FText::FromString(TEXT("推箱子关卡编辑器")), FText::FromString(TEXT("选择格子配置关卡，校验并保存数据资产。")), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(SokobanTabId); })));
	}

	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args)
	{
		// 内容浏览器若已选中关卡资产，打开窗口时直接载入，省去再次选择。
		TArray<FAssetData> SelectedAssets;
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(SelectedAssets);
		USokobanLevelData* InitialAsset = nullptr;
		for (const auto& Asset : SelectedAssets)
		{
			InitialAsset = Cast<USokobanLevelData>(Asset.GetAsset());
			if (InitialAsset) { break; }
		}
		TSharedRef<SSokobanLevelEditor> Editor = SNew(SSokobanLevelEditor).InitialAsset(InitialAsset);
		TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(ETabRole::NomadTab)[Editor];
		Tab->SetCanCloseTab(SDockTab::FCanCloseTab::CreateSP(Editor, &SSokobanLevelEditor::CanClose));
		return Tab;
	}
	bool bRegistered = false;
};

IMPLEMENT_MODULE(FSokobanEditorModule, SokobanEditor)
