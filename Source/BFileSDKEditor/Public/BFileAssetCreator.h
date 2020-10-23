/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "BFileCommonTypes.h"
#include "BFileFinalTypes.h"

class BFileAssetCreator
{
private:
	BFinalAssetContent* AssetPtr;

	TFunction<void(TFunction<void()>)> TaskQueuer;

	FCriticalSection HierarchyIDToNodeMap_Mutex;
	FCriticalSection GeometryIDToNodeMap_Mutex;
	FCriticalSection MetadataIDToNodeMap_Mutex;

	TMap<uint64, TSharedPtr<FCriticalSection, ESPMode::ThreadSafe>> HierarchyID_ChildrenMutex_Map; //Secured by HierarchyIDToNodeMap_Mutex

	TWeakPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> GetOrInsertHierarchyNode(uint64 InNodeID);
	TWeakPtr<BFinalGeometryNode, ESPMode::ThreadSafe> GetOrInsertGeometryNode(uint64 InNodeID);
	TWeakPtr<BFinalMetadataNode, ESPMode::ThreadSafe> GetOrInsertMetadataNode(uint64 InNodeID);

	void NewHierarchyNode(const class BHierarchyNode& InNewNode);
	void NewGeometryNode(const class BGeometryNode& InNewNode);
	void NewMetadataNode(const class BMetadataNode& InNewNode);

	static void BuildStaticMesh(class UStaticMesh* StaticMesh, class FEvent* DoneEvent);

	FThreadSafeCounter ActiveTaskCount;

public:
	BFileAssetCreator(class BFinalAssetContent* InAssetPtr, TFunction<void(TFunction<void()>)> InTaskQueuer);

	void ProvideNewNode(const class BHierarchyNode& InNode);
	void ProvideNewNode(const class BGeometryNode& InNode);
	void ProvideNewNode(const class BMetadataNode& InNode);

	bool IsCompleted() const
	{
		return ActiveTaskCount.GetValue() == 0;
	}
};