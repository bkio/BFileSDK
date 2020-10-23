/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileAssetActorManager.h"
#include "BFileAssetActor.h"
#include "BFileAssetStateHolderActor.h"
#if WITH_EDITOR
#include "Editor.h"
#include "EngineUtils.h"
#endif

UBFileAssetActorManager::UBFileAssetActorManager()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) return;
#if WITH_EDITOR
	TWeakObjectPtr<UBFileAssetActorManager> ThisPtr(this);

	FEditorDelegates::PreSaveWorld.AddUObject(this, &UBFileAssetActorManager::OnPreSaveWorld);
	FEditorDelegates::PostSaveWorld.AddUObject(this, &UBFileAssetActorManager::OnPostSaveWorld);

	FEditorDelegates::BeginPIE.AddUObject(this, &UBFileAssetActorManager::OnBeginPIE);
	FEditorDelegates::PostPIEStarted.AddUObject(this, &UBFileAssetActorManager::OnPostPIEStarted);

	if (GEditor)
	{
		FWorldDelegates::OnPostWorldInitialization.AddLambda([ThisPtr]
		(UWorld* World, const UWorld::InitializationValues WorldInitValues)
			{
				if (ThisPtr.IsValid())
				{
					GEditor->OnLevelActorDeleted().RemoveAll(ThisPtr.Get());
					ThisPtr->OnPostSaveWorld(0, World, true);
					GEditor->OnLevelActorDeleted().AddUObject(ThisPtr.Get(), &UBFileAssetActorManager::OnActorDeletedInEditor);
				}
			});
	}
#endif
}
UBFileAssetActorManager::~UBFileAssetActorManager()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) return;
#if WITH_EDITOR
	FEditorDelegates::PreSaveWorld.RemoveAll(this);
	FEditorDelegates::PostSaveWorld.RemoveAll(this);
	FEditorDelegates::BeginPIE.RemoveAll(this);
	FEditorDelegates::PostPIEStarted.RemoveAll(this);
	FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);
#endif
}

void UBFileAssetActorManager::InitializeManager() //Called once from StartupModule
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) return;
}

#if WITH_EDITOR
void UBFileAssetActorManager::OnPreSaveWorld(uint32 SaveFlags, UWorld* World)
{
	bWorldBeingSavedOrPIEStarting = true;

	TArray<ABFileAssetActor*> Actors;
	for (TActorIterator<ABFileAssetActor> It(World); It; ++It)
	{
		Actors.Add(*It);
	}

	for (int32 i = 0; i < Actors.Num(); i++)
	{
		if (Actors[i] && Actors[i]->IsValidLowLevel())
		{
			//if (Actors[i]->StateHolderActorWeakPtr.IsValid())
			//{
			//	(ABFileAssetStateHolderActor*)Actors[i]->StateHolderActorWeakPtr.Get();
			//}

			Actors[i]->Destroy(true, true);
			Actors[i]->MarkPendingKill();
		}
	}
}

void UBFileAssetActorManager::OnPostSaveWorld(uint32 SaveFlags, UWorld* World, bool bSuccess)
{
	TArray<ABFileAssetStateHolderActor*> StateActors;
	for (TActorIterator<ABFileAssetStateHolderActor> It(World); It; ++It)
	{
		StateActors.Add(*It);
	}

	for (int32 i = 0; i < StateActors.Num(); i++)
	{
		if (StateActors[i] && StateActors[i]->IsValidLowLevel())
		{
			TSharedPtr<BFinalAssetContent> DeserializedContent = MakeShareable(new BFinalAssetContent());
			DeserializedContent->XDeserialize(StateActors[i]->SerializedContent);

			auto SpawnedActor = SpawnXAsset(
				StateActors[i],
				StateActors[i]->SerializedContent,
				DeserializedContent,
				StateActors[i]->LastCachedAssetActorTransform,
				false/*bSpawnStateActorAndMakeLevelDirty_InEditor*/);

			SpawnedActor->StateHolderActorWeakPtr = StateActors[i];
			StateActors[i]->AssetActorWeakPtr = SpawnedActor;
		}
	}

	bWorldBeingSavedOrPIEStarting = false;
}

void UBFileAssetActorManager::OnBeginPIE(bool bIsSimulating)
{
	if (GEditor)
	{
		OnPreSaveWorld(0, GEditor->GetEditorWorldContext().World());
	}
}

void UBFileAssetActorManager::OnPostPIEStarted(bool bIsSimulating)
{
	if (GEditor)
	{
		OnPostSaveWorld(0, GEditor->GetEditorWorldContext().World(), true);
	}
}

void UBFileAssetActorManager::OnActorDeletedInEditor(AActor* Actor)
{
	if (Actor->IsA(ABFileAssetActor::StaticClass()))
	{
		if (IsWorldBeingSavedOrPIEStarting()) return;

		auto CastedActor = (ABFileAssetActor*)Actor;
		if (CastedActor->StateHolderActorWeakPtr.IsValid())
		{
			auto CastedStateActor = (ABFileAssetStateHolderActor*)CastedActor->StateHolderActorWeakPtr.Get();
			CastedStateActor->bAssetActorDeletedInEditor = true;
			CastedStateActor->Destroy(true, true);
		}
	}
	else if (Actor->IsA(ABFileAssetStateHolderActor::StaticClass()))
	{
		ABFileAssetStateHolderActor* CastedActor = (ABFileAssetStateHolderActor*)Actor;

		if (CastedActor->bAssetActorDeletedInEditor) return;

		auto CastedAssetActor = (ABFileAssetActor*)CastedActor->AssetActorWeakPtr.Get();
		CastedAssetActor->SpawnStateActorAndMakeLevelDirty(CastedActor->SerializedContent);
	}
}

bool UBFileAssetActorManager::bWorldBeingSavedOrPIEStarting = false;
bool UBFileAssetActorManager::IsWorldBeingSavedOrPIEStarting()
{
	return bWorldBeingSavedOrPIEStarting;
}
#endif