/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License
/// https://lxjk.github.io/2019/10/01/How-to-Make-Tools-in-U-E.html

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"

enum class EBFileAssetRenderComponentType : uint8;

struct HBFileHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( BFILESDK_API );

	HBFileHitProxy(class UBFileAssetHISMComponent* InHISMComponent, int32 InIndex);
	HBFileHitProxy(class UBFileAssetSMComponent* InSMComponent, int32 InIndex);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual EMouseCursor::Type GetMouseCursor() override;

	class ABFileAssetActor* BFileAssetActor;
	EBFileAssetRenderComponentType Which;
	class UBFileAssetHISMComponent* HISMComponent;
	class UBFileAssetSMComponent* SMComponent;
	int32 Index;
};