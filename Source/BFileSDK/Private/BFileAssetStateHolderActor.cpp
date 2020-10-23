/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileAssetStateHolderActor.h"
#include "BFileAssetActorManager.h"
#include "BFileFinalTypes.h"

ABFileAssetStateHolderActor::ABFileAssetStateHolderActor() : AActor()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) return;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
#if WITH_EDITOR
	bListedInSceneOutliner = false;
#endif
}

#if WITH_EDITOR
#else
void ABFileAssetStateHolderActor::BeginPlay()
{
	AActor::BeginPlay();

	TSharedPtr<BFinalAssetContent> DeserializedContent = MakeShareable(new BFinalAssetContent);
	DeserializedContent->XDeserialize(SerializedContent);

	UBFileAssetActorManager::SpawnXAsset(this, SerializedContent, DeserializedContent, LastCachedAssetActorTransform);

	Destroy(true, true);
	MarkPendingKill();
}
#endif