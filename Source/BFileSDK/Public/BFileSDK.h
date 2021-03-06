/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FBFileSDKModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;

private:
	TWeakObjectPtr<class UBFileAssetActorManager> BFileAssetActorManager;
};