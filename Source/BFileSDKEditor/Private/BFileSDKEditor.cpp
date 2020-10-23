/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileSDKEditor.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "BFileAssetActions.h"

void FBFileSDKEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FBFileAssetActions));
}

void FBFileSDKEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (int32 i = 0; i < CreatedAssetTypeActions.Num(); ++i)
		{
			AssetTools.UnregisterAssetTypeActions(CreatedAssetTypeActions[i].ToSharedRef());
		}
	}

	CreatedAssetTypeActions.Empty();
}

void FBFileSDKEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}
	
IMPLEMENT_MODULE(FBFileSDKEditorModule, BFileSDKEditor)