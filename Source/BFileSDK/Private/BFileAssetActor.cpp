/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileAssetActor.h"
#include "BFileAsset.h"
#include "BFileFinalTypes.h"
#include "BFileMeshSerialization.h"
#include "BFileAssetActorManager.h"
#include "BFileAssetStateHolderActor.h"
#include "BFileAssetRenderComponents.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "BLambdaRunnable.h"

UBFileAssetRootComponent::UBFileAssetRootComponent() : Super() {}

#if WITH_EDITOR
void UBFileAssetRootComponent::PostEditComponentMove(bool bFinished)
{
    Super::PostEditComponentMove(bFinished);

    OnRootComponentMovedInEditor.Broadcast();
}

ABFileAssetStateHolderActor* ABFileAssetActor::SpawnStateActorAndMakeLevelDirty(const TArray<uint8>& SerializedContent)
{
	auto StateActor = UBFileAssetActorManager::SpawnActorInternal<ABFileAssetStateHolderActor>(GetWorld(), ABFileAssetStateHolderActor::StaticClass(), FTransform());
	StateActor->SerializedContent = SerializedContent;
	StateActor->LastCachedAssetActorTransform = GetActorTransform();

	StateActor->AssetActorWeakPtr = this;
	StateHolderActorWeakPtr = StateActor;

	GetWorld()->MarkPackageDirty();

	return StateActor;
}
#endif

ABFileAssetActor::ABFileAssetActor() : AActor()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) return;

	UBFileAssetRootComponent* NewRootComponent = CreateDefaultSubobject<UBFileAssetRootComponent>(TEXT("RootComponent"));
#if WITH_EDITOR
	NewRootComponent->OnRootComponentMovedInEditor.AddUObject(this, &ABFileAssetActor::OnRootComponentHasMoved);
#endif
	RootComponent = NewRootComponent;
	RootComponent->SetMobility(EComponentMobility::Movable);

	ConstructorHelpers::FObjectFinder<UMaterialInterface> MeshMaterialAsset_Instanced(TEXT("/BFileSDK/Materials/MI_MeshMaterial_Instanced.MI_MeshMaterial_Instanced"));
	if (MeshMaterialAsset_Instanced.Succeeded())
	{
		MeshMaterial_Instanced = MeshMaterialAsset_Instanced.Object;
	}

	ConstructorHelpers::FObjectFinder<UMaterialInterface> MeshMaterialAsset_NonInstanced(TEXT("/BFileSDK/Materials/MI_MeshMaterial_NonInstanced.MI_MeshMaterial_NonInstanced"));
	if (MeshMaterialAsset_NonInstanced.Succeeded())
	{
		MeshMaterial_NonInstanced = MeshMaterialAsset_NonInstanced.Object;
	}
}

ABFileAssetActor::~ABFileAssetActor()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) return;
}

template<class T>
T* UBFileAssetActorManager::SpawnActorInternal(UWorld* SpawnInWorld, UClass* ActorClass, const FTransform& Transform)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.bNoFail = true;
	SpawnInfo.bAllowDuringConstructionScript = true;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	return SpawnInWorld->SpawnActor<T>(T::StaticClass(), Transform, SpawnInfo);
}

ABFileAssetActor* UBFileAssetActorManager::SpawnXAsset(
	UObject* WorldContextObject, 
	UBFileAsset* BFileAsset, 
	const FTransform& InitialTransform)
{
	if (!BFileAsset->DeserializedContent.IsValid())
	{
		BFileAsset->DeserializedContent = MakeShareable(new BFinalAssetContent);
		BFileAsset->DeserializedContent->XDeserialize(BFileAsset->SerializedContent);
	}
	return SpawnXAsset(WorldContextObject, BFileAsset->SerializedContent, BFileAsset->DeserializedContent, InitialTransform);
}

ABFileAssetActor* UBFileAssetActorManager::SpawnXAsset(
	UObject* WorldContextObject, 
	const TArray<uint8>& SerializedContent, 
	TSharedPtr<BFinalAssetContent> DeserializedContent, 
	const FTransform& InitialTransform
#if WITH_EDITOR
	, bool bSpawnStateActorAndMakeLevelDirty_InEditor
#endif
	)
{
	if (SerializedContent.Num() == 0) return nullptr;
	if (!DeserializedContent.IsValid()) return nullptr;
	if (!DeserializedContent->RootNode.IsValid()) return nullptr;

	auto Actor = SpawnActorInternal<ABFileAssetActor>(WorldContextObject->GetWorld(), ABFileAssetActor::StaticClass(), InitialTransform);
	Actor->DeserializedContent = DeserializedContent;

#if WITH_EDITOR
	if (bSpawnStateActorAndMakeLevelDirty_InEditor)
	{
		Actor->SpawnStateActorAndMakeLevelDirty(SerializedContent);
	}
#endif

	FThreadSafeCounter* ParallelTaskCounter = new FThreadSafeCounter(Actor->DeserializedContent->GeometryIDToNodeMap.Num());
	FEvent* ParallelTaskCounter_CompletedEvent = FGenericPlatformProcess::GetSynchEventFromPool();

	for (auto& GPair : Actor->DeserializedContent->GeometryIDToNodeMap)
	{
		int64 GUniqueID = GPair.Key;
		auto GSerializedRenderDataPtr = &GPair.Value->SerializedRenderData;

		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Actor);
		Actor->GeometryID_StaticMesh_Map.Add(GUniqueID, StaticMesh);

		Actor->GeometryID_MeshComponent_Map.Add(GUniqueID, NewObject<UBSMCSwitcher>(Actor));

		FBLambdaRunnable::RunLambdaOnBackgroundThreadPool([GUniqueID, GSerializedRenderDataPtr, StaticMesh, ParallelTaskCounter, ParallelTaskCounter_CompletedEvent]()
			{
				const TArray<uint8>& SerializedRenderData = *GSerializedRenderDataPtr;
				BFileMeshSerialization::DeserializeToStaticMesh_ExecuteThreadablePart(StaticMesh, SerializedRenderData);
				{
					if (ParallelTaskCounter->Decrement() == 0) ParallelTaskCounter_CompletedEvent->Trigger();
				}
			});
	}

	ParallelTaskCounter_CompletedEvent->Wait();
	FGenericPlatformProcess::ReturnSynchEventToPool(ParallelTaskCounter_CompletedEvent);

	delete ParallelTaskCounter;

	for (auto& GMPair : Actor->GeometryID_StaticMesh_Map)
	{
		BFileMeshSerialization::DeserializeToStaticMesh_ExecutePostThreadablePart(GMPair.Value);
	}

	TMap<UBSMCSwitcher*, TWeakPtr<BFinalGeometryPart>> OneGPartOccurrenceMap;
	TArray<UBFileAssetHISMComponent*> HISMCList;
	Actor->InitializeRecursive(DeserializedContent->RootNode, OneGPartOccurrenceMap, HISMCList);

	//Get all SMC for the actor and setup
	for (auto& SMCPair : OneGPartOccurrenceMap)
	{
		TSharedPtr<BFinalGeometryPart> GPart = SMCPair.Value.Pin();

		auto StaticMesh = Actor->GeometryID_StaticMesh_Map[GPart->GeometryNode.Pin()->UniqueID];

		SMCPair.Key->SMC = NewObject<UBFileAssetSMComponent>(Actor);
		SMCPair.Key->SMC->SetStaticMesh(StaticMesh);

		if (Actor->MeshMaterial_NonInstanced)
		{
			UMaterialInstanceDynamic* MeshMaterialDynamicInstance = UMaterialInstanceDynamic::Create(Actor->MeshMaterial_NonInstanced, Actor);
			SMCPair.Key->SMC->SetMaterial(0, MeshMaterialDynamicInstance);
			MeshMaterialDynamicInstance->SetScalarParameterValue("ColorCompressed", ABFileAssetActor::CompressColorAsSingleFloat(SMCPair.Value.Pin()->Color));
		}
		
		Actor->AddOwnedComponent(SMCPair.Key->SMC);

		SMCPair.Key->SMC->SetupAttachment(Actor->GetRootComponent());
		SMCPair.Key->SMC->RegisterComponent();

		SMCPair.Key->SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	//Get all HISMC for the actor and mark render state dirty/build tree
	for (UBFileAssetHISMComponent* HISMC : HISMCList)
	{
		HISMC->ReleasePerInstanceRenderData();
		HISMC->InitPerInstanceRenderData(true);
		HISMC->MarkRenderStateDirty();
		HISMC->BuildTreeIfOutdated(true, false);
	}

	return Actor;
}

void ABFileAssetActor::InitializeRecursive(
	TWeakPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> Node, 
	TMap<UBSMCSwitcher*, TWeakPtr<BFinalGeometryPart>>& OneGPartOccurrenceMap, 
	TArray<UBFileAssetHISMComponent*>& HISMCList)
{
	auto Pinned = Node.Pin();

	for (TSharedPtr<BFinalGeometryPart> GPart : Pinned->Geometries)
	{
		int64 GUniqueID = GPart->GeometryNode.Pin()->UniqueID;
		UBSMCSwitcher* GPartSMC = GeometryID_MeshComponent_Map[GUniqueID];

		GPartSMC->NumOccurrence++;

		//Time for creating HISMC
		if (GPartSMC->NumOccurrence == 2)
		{
			auto StaticMesh = GeometryID_StaticMesh_Map[GUniqueID];

			GPartSMC->ComponentType = EBFileAssetRenderComponentType::HISMC;

			GPartSMC->HISMC = NewObject<UBFileAssetHISMComponent>(this);
			HISMCList.Add(GPartSMC->HISMC);

			GPartSMC->HISMC->SetStaticMesh(StaticMesh);
			if (MeshMaterial_Instanced)
			{
				GPartSMC->HISMC->SetMaterial(0, MeshMaterial_Instanced);
			}

			AddOwnedComponent(GPartSMC->HISMC);

			GPartSMC->HISMC->SetupAttachment(GetRootComponent());
			GPartSMC->HISMC->RegisterComponent();

			GPartSMC->HISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GPartSMC->HISMC->bHasPerInstanceHitProxies = true;
			GPartSMC->HISMC->NumCustomDataFloats = 1;

			TSharedPtr<BFinalGeometryPart> FirstGPart = OneGPartOccurrenceMap.FindAndRemoveChecked(GPartSMC).Pin();

			FirstGPart->InstanceIndex = GPartSMC->HISMC->PerInstanceSMData.Add(FirstGPart->Transform.ToMatrixWithScale());
			GPartSMC->HISMC->PerInstanceSMCustomData.Add(CompressColorAsSingleFloat(FirstGPart->Color));

			GPart->InstanceIndex = GPartSMC->HISMC->PerInstanceSMData.Add(GPart->Transform.ToMatrixWithScale());

			GPartSMC->HISMC->PerInstanceSMCustomData.Add(CompressColorAsSingleFloat(GPart->Color));
		}
		//Add new instance to HISMC
		else if (GPartSMC->NumOccurrence > 2)
		{
			GPart->InstanceIndex = GPartSMC->HISMC->PerInstanceSMData.Add(GPart->Transform.ToMatrixWithScale());

			GPartSMC->HISMC->PerInstanceSMCustomData.Add(CompressColorAsSingleFloat(GPart->Color));
		}					
		else // if (GPartSMC.NumOccurrence == 1) /*Only option left*/
		{
			GPartSMC->ComponentType = EBFileAssetRenderComponentType::SMC;

			OneGPartOccurrenceMap.Add(GPartSMC, GPart);
		}
	}

	for (int32 i = 0; i < Pinned->Children.Num(); i++)
	{
		InitializeRecursive(Pinned->Children[i], OneGPartOccurrenceMap, HISMCList);
	}
}

float ABFileAssetActor::CompressColorAsSingleFloat(const FColor& Color, uint8 Reserved7Bit)
{
	uint32 Result = uint32(Color.R);
	Result |= uint32(Color.G) << 8;
	Result |= uint32(Color.B) << 24;
	Result |= (uint32(Reserved7Bit) & 0x0000007F) << 16;
	if ((Result ^ 0x7F000000) == 0x00000000)
	{
		Result |= 0x00800000;
	}
	return *reinterpret_cast<float*>(&Result);
}

#if WITH_EDITOR
void ABFileAssetActor::OnRootComponentHasMoved()
{
	if (StateHolderActorWeakPtr.IsValid())
	{
		auto CastedStateActor = (ABFileAssetStateHolderActor*)StateHolderActorWeakPtr.Get();
		CastedStateActor->LastCachedAssetActorTransform = GetActorTransform();
	}
}
#endif