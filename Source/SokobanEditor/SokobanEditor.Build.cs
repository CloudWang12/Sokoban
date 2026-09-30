using UnrealBuildTool;

public class SokobanEditor : ModuleRules
{
	public SokobanEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine", "Sokoban", "Slate", "SlateCore",
			"UnrealEd", "ToolMenus", "ContentBrowser", "AssetTools", "AssetRegistry", "PropertyEditor", "InputCore",
			"UMG", "UMGEditor", "RenderCore", "ImageCore"
		});
	}
}
