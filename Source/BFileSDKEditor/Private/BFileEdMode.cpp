/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License
/// https://lxjk.github.io/2019/10/01/How-to-Make-Tools-in-U-E.html

#if WITH_EDITOR

#include "BFileEdMode.h"
#include "BFileHitProxy.h"
#include "BFileAssetActor.h"
#include "BFileAssetRenderComponents.h"

bool FBFileEdMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	bool bIsHandled = false;

	if (HitProxy)
	{
		if (HitProxy->IsA(HBFileHitProxy::StaticGetType()))
		{
			HBFileHitProxy* BFileHitProxy = (HBFileHitProxy*)HitProxy;
			if (BFileHitProxy)
			{
				AActor* OwnerActor = nullptr;

				if (BFileHitProxy->Which == EBFileAssetRenderComponentType::HISMC)
				{
					OwnerActor = BFileHitProxy->HISMComponent->GetOwner();
				}
				else if (BFileHitProxy->Which == EBFileAssetRenderComponentType::SMC)
				{
					OwnerActor = BFileHitProxy->SMComponent->GetOwner();
				}

				if (OwnerActor && OwnerActor->IsA(ABFileAssetActor::StaticClass()))
				{
					ABFileAssetActor* CastedActor = (ABFileAssetActor*)OwnerActor;
					bIsHandled = true;
				}
			}
		}
	}

	return bIsHandled;
}

#endif