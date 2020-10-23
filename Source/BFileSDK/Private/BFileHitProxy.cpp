/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License
/// https://lxjk.github.io/2019/10/01/How-to-Make-Tools-in-U-E.html

#include "BFileHitProxy.h"
#include "BFileAssetActor.h"
#include "BFileAssetRenderComponents.h"

HBFileHitProxy::HBFileHitProxy(UBFileAssetHISMComponent* InHISMComponent, int32 InIndex) 
	: HHitProxy(HPP_World)
	, Which(EBFileAssetRenderComponentType::HISMC)
	, HISMComponent(InHISMComponent)
	, Index(InIndex)
{
	BFileAssetActor = InHISMComponent->GetOwner()->IsA(ABFileAssetActor::StaticClass()) ? (ABFileAssetActor*)InHISMComponent->GetOwner() : nullptr;
}
HBFileHitProxy::HBFileHitProxy(UBFileAssetSMComponent* InSMComponent, int32 InIndex) 
	: HHitProxy(HPP_World)
	, Which(EBFileAssetRenderComponentType::SMC)
	, SMComponent(InSMComponent)
	, Index(InIndex) 
{
	BFileAssetActor = InSMComponent->GetOwner()->IsA(ABFileAssetActor::StaticClass()) ? (ABFileAssetActor*)InSMComponent->GetOwner() : nullptr;
}

void HBFileHitProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (BFileAssetActor == nullptr) return;

	if (Which == EBFileAssetRenderComponentType::HISMC)
		Collector.AddReferencedObject(HISMComponent);
	else if (Which == EBFileAssetRenderComponentType::SMC)
		Collector.AddReferencedObject(SMComponent);
}

EMouseCursor::Type HBFileHitProxy::GetMouseCursor()
{
	return EMouseCursor::CardinalCross;
}

IMPLEMENT_HIT_PROXY(HBFileHitProxy, HHitProxy);
