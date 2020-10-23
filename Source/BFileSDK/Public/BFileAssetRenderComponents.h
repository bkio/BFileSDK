/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "BFileAssetRenderComponents.generated.h"

UENUM(BlueprintType)
enum class EBFileAssetRenderComponentType : uint8
{
	None = 0,
	HISMC = 1,
	SMC = 2
};

UCLASS()
class BFILESDK_API UBFileAssetHISMComponent : public UHierarchicalInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
};

UCLASS()
class BFILESDK_API UBFileAssetSMComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
};

UCLASS(BlueprintType)
class BFILESDK_API UBSMCSwitcher : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BFileSDK")
	EBFileAssetRenderComponentType ComponentType = EBFileAssetRenderComponentType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BFileSDK")
	UBFileAssetHISMComponent* HISMC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BFileSDK")
	UBFileAssetSMComponent* SMC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BFileSDK")
	int32 NumOccurrence = 0;
};