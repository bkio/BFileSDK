/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BFileFinalTypes.h"
#include "BFileAssetActorManager.generated.h"

UCLASS()
class BFILESDK_API UBFileAssetActorManager : public UObject
{
	GENERATED_BODY()

public:
	UBFileAssetActorManager();
	~UBFileAssetActorManager();

	UFUNCTION(BlueprintCallable, Category = "BFileSDK", meta = (WorldContext = "WorldContextObject"))
	static ABFileAssetActor* SpawnXAsset(
		UObject* WorldContextObject,
		class UBFileAsset* BFileAsset,
		const FTransform& InitialTransform);

	static ABFileAssetActor* SpawnXAsset(
		UObject* WorldContextObject,
		const TArray<uint8>& SerializedContent,
		TSharedPtr<class BFinalAssetContent> DeserializedContent,
		const FTransform& InitialTransform
#if WITH_EDITOR
		, bool bSpawnStateActorAndMakeLevelDirty_InEditor = true
#endif
	);

#if WITH_EDITOR
	static bool IsWorldBeingSavedOrPIEStarting();
#endif

private:
	friend class FBFileSDKModule;
	friend class ABFileAssetActor;

	void InitializeManager(); //Called from FBFileSDKModule

	template<class T>
	static T* SpawnActorInternal(class UWorld* SpawnInWorld, class UClass* ActorClass, const FTransform& Transform);

#if WITH_EDITOR
	UFUNCTION()
	void OnPreSaveWorld(uint32 SaveFlags, class UWorld* World);

	UFUNCTION()
	void OnPostSaveWorld(uint32 SaveFlags, class UWorld* World, bool bSuccess);

	UFUNCTION()
	void OnBeginPIE(bool bIsSimulating);

	UFUNCTION()
	void OnPostPIEStarted(bool bIsSimulating);

	UFUNCTION()
	void OnActorDeletedInEditor(class AActor* Actor);

	static bool bWorldBeingSavedOrPIEStarting;
#endif
};