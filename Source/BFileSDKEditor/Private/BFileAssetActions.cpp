/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileAssetActions.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/SlateStyle.h"
#include "BFileAsset.h"
#include "BFileAssetActorManager.h"
#include "BLambdaRunnable.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

/* FBFileAssetActions constructors
 *****************************************************************************/

FBFileAssetActions::FBFileAssetActions()
{ }


/* FAssetTypeActions_Base overrides
 *****************************************************************************/

bool FBFileAssetActions::CanFilter()
{
	return true;
}

void FBFileAssetActions::GetActions(const TArray<UObject*>& InSelectedAssets, FMenuBuilder& MenuBuilder)
{
	FAssetTypeActions_Base::GetActions(InSelectedAssets, MenuBuilder);

	auto SelectedBFileAssets = GetTypedWeakObjectPtrs<UBFileAsset>(InSelectedAssets);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("BFileAsset_Spawn", "Spawn XAsset"),
		LOCTEXT("BFileAsset_SpawnToolTip", "Spawn the XAsset into the current scene."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]
			{
				for (auto& BFileAsset : SelectedBFileAssets)
				{
					if (BFileAsset.IsValid() && BFileAsset->SerializedContent.Num() > 0)
					{
						FBLambdaRunnable::RunLambdaOnGameThread([BFileAsset]()
							{
								if (GEditor)
								{
									UBFileAssetActorManager::SpawnXAsset(GEditor->GetEditorWorldContext().World(), BFileAsset.Get(), FTransform());
								}
							});
					}
				}
			}),
			FCanExecuteAction::CreateLambda([=] 
			{
				for (auto& BFileAsset : SelectedBFileAssets)
				{
					if (BFileAsset.IsValid() && BFileAsset->SerializedContent.Num() > 0)
					{
						return true;
					}
				}
				return false;
			})
		)
	);
}

uint32 FBFileAssetActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

FText FBFileAssetActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_BFileAsset", "BFileAsset");
}

UClass* FBFileAssetActions::GetSupportedClass() const
{
	return UBFileAsset::StaticClass();
}

FColor FBFileAssetActions::GetTypeColor() const
{
	return FColor((uint8)91, (uint8)198, (uint8)232);
}

bool FBFileAssetActions::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

#undef LOCTEXT_NAMESPACE