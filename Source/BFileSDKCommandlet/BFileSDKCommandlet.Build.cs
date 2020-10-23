/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

namespace UnrealBuildTool.Rules
{
	public class BFileSDKCommandlet : ModuleRules
	{
		public BFileSDKCommandlet(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
				});

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "Json",
                    "JsonUtilities",
                    "HTTP",
					"BHttpClientLib",
					"BZipLib",
					"BUtilities",
					"BFileSDK",
					"BFileSDKEditor"
                });
        }
	}
}
