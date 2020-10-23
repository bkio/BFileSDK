/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileSDK.h"
#include "BFileAssetActorManager.h"

void FBFileSDKModule::StartupModule()
{
	BFileAssetActorManager = NewObject<UBFileAssetActorManager>(GetTransientPackage(), NAME_None, RF_Public | RF_Standalone | RF_MarkAsRootSet);
	BFileAssetActorManager->ClearFlags(RF_Transactional);
	BFileAssetActorManager->InitializeManager();
}

IMPLEMENT_MODULE(FBFileSDKModule, BFileSDK)