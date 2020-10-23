/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FBFileSDKEditorModule : public IModuleInterface
{

private:
	void RegisterAssetTypeAction(class IAssetTools& AssetTools, TSharedRef<class IAssetTypeActions> Action);

	TArray<TSharedPtr<class IAssetTypeActions>> CreatedAssetTypeActions;

public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};