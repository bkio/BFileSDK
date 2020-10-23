/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

using UnrealBuildTool;

public class BFileSDK : ModuleRules
{
	public BFileSDK(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Json", "BZipLib", "BUtilities" });

        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

        if (Target.Type == TargetRules.TargetType.Editor)
        {
            PublicDependencyModuleNames.AddRange(new string[] { "UnrealEd" });
        }
	}
}