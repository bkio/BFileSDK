/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License
/// https://lxjk.github.io/2019/10/01/How-to-Make-Tools-in-U-E.html

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "HitProxies.h"
#include "EdMode.h"
#include "EditorViewportClient.h"

class BFILESDKEDITOR_API FBFileEdMode : public FEdMode
{
public:
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;

	/*virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual bool CapturedMouseMove(FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY) override;
	virtual bool StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual void PostUndo() override;
	virtual void ActorsDuplicatedNotify(TArray<AActor*>& PreDuplicateSelection, TArray<AActor*>& PostDuplicateSelection, bool bOffsetLocations) override;
	virtual void ActorMoveNotify() override;
	virtual void ActorSelectionChangeNotify() override;
	virtual void MapChangeNotify() override;
	virtual void SelectionChanged() override;*/
};

#endif