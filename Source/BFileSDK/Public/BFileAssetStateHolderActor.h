/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BFileAssetStateHolderActor.generated.h"

UCLASS()
class BFILESDK_API ABFileAssetStateHolderActor : public AActor
{
	GENERATED_BODY()

public:
	ABFileAssetStateHolderActor();

	UPROPERTY()
	TArray<uint8> SerializedContent;

	UPROPERTY()
	FTransform LastCachedAssetActorTransform;

#if WITH_EDITOR
	TWeakObjectPtr<AActor> AssetActorWeakPtr;
	bool bAssetActorDeletedInEditor = false;
#else
	virtual void BeginPlay() override;
#endif
};