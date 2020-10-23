/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

using UnrealBuildTool;

public class BFileSDKEditor : ModuleRules
{
	public BFileSDKEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] 
		{ 
			"Core", "CoreUObject", "Engine", "Slate", "SlateCore", "UnrealEd", 
			"BFileSDK", "BUtilities", "BZipLib", "RawMesh", "MeshBuilder"
		});

        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}