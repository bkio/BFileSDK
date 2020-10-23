/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "BFileAssetActor.generated.h"

UCLASS(BlueprintType)
class BFILESDK_API UBFileAssetRootComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UBFileAssetRootComponent();

#if WITH_EDITOR
	virtual void PostEditComponentMove(bool bFinished) override;

	DECLARE_MULTICAST_DELEGATE(FBFileAssetRootComponentMovedInEditor);
	FBFileAssetRootComponentMovedInEditor OnRootComponentMovedInEditor;
#endif
};

UCLASS(BlueprintType)
class BFILESDK_API ABFileAssetActor : public AActor
{
	GENERATED_BODY()

public:
	ABFileAssetActor();
	~ABFileAssetActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BFileSDK")
	TMap<int64, class UBSMCSwitcher*> GeometryID_MeshComponent_Map;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BFileSDK")
	TMap<int64, class UStaticMesh*> GeometryID_StaticMesh_Map;
	
	TSharedPtr<class BFinalAssetContent> DeserializedContent;

#if WITH_EDITOR
	TWeakObjectPtr<AActor> StateHolderActorWeakPtr;

	class ABFileAssetStateHolderActor* SpawnStateActorAndMakeLevelDirty(const TArray<uint8>& SerializedContent);
#endif

private:
	UPROPERTY()
	class UMaterialInterface* MeshMaterial_Instanced = nullptr;

	UPROPERTY()
	class UMaterialInterface* MeshMaterial_NonInstanced = nullptr;

#if WITH_EDITOR
	UFUNCTION()
	void OnRootComponentHasMoved();
#endif

	void InitializeRecursive(
		TWeakPtr<class BFinalHiearchyNode, ESPMode::ThreadSafe> Node, 
		TMap<UBSMCSwitcher*, TWeakPtr<class BFinalGeometryPart>>& OneGPartOccurrenceMap, 
		TArray<class UBFileAssetHISMComponent*>& HISMCList);

	static float CompressColorAsSingleFloat(const FColor& Color, uint8 Reserved7Bit = 0);

	friend class UBFileAssetActorManager;
};