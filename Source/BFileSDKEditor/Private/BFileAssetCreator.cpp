/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileAssetCreator.h"
#include "BFileAssetFactory.h"
#include "BFileAsset.h"
#include "BFileFinalTypes.h"
#include "BFileMeshSerialization.h"
#include "BLambdaRunnable.h"
#include "Engine/StaticMesh.h"
#include "Runtime/RawMesh/Public/RawMesh.h"
#include "PhysicsEngine/BodySetup.h"

BFileAssetCreator::BFileAssetCreator(BFinalAssetContent* InAssetPtr, TFunction<void(TFunction<void()>)> InTaskQueuer)
{
	AssetPtr = InAssetPtr;
	TaskQueuer = InTaskQueuer;
}

void BFileAssetCreator::ProvideNewNode(const BHierarchyNode& InNode)
{
	ActiveTaskCount.Increment();
	FBLambdaRunnable::RunLambdaOnBackgroundThreadPool([this, InNode]()
		{
			NewHierarchyNode(InNode);
			ActiveTaskCount.Decrement();
		});
}
void BFileAssetCreator::ProvideNewNode(const BGeometryNode& InNode)
{
	ActiveTaskCount.Increment();
	FBLambdaRunnable::RunLambdaOnBackgroundThreadPool([this, InNode]()
		{
			NewGeometryNode(InNode);
			ActiveTaskCount.Decrement();
		});
}
void BFileAssetCreator::ProvideNewNode(const BMetadataNode& InNode)
{
	ActiveTaskCount.Increment();
	FBLambdaRunnable::RunLambdaOnBackgroundThreadPool([this, InNode]()
		{
			NewMetadataNode(InNode);
			ActiveTaskCount.Decrement();
		});
}

TWeakPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> BFileAssetCreator::GetOrInsertHierarchyNode(uint64 InNodeID)
{
	FScopeLock Lock(&HierarchyIDToNodeMap_Mutex);
	TSharedPtr<BFinalHiearchyNode, ESPMode::ThreadSafe>* Found = AssetPtr->HierarchyIDToNodeMap.Find(InNodeID);
	if (Found != nullptr) return *Found;

	TSharedPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> NewNode = MakeShareable(new BFinalHiearchyNode);
	NewNode->UniqueID = InNodeID;

	HierarchyID_ChildrenMutex_Map.Add(InNodeID, MakeShareable(new FCriticalSection));

	AssetPtr->HierarchyIDToNodeMap.Add(InNodeID, NewNode);

	return NewNode;
}
TWeakPtr<BFinalGeometryNode, ESPMode::ThreadSafe> BFileAssetCreator::GetOrInsertGeometryNode(uint64 InNodeID)
{
	FScopeLock Lock(&GeometryIDToNodeMap_Mutex);
	TSharedPtr<BFinalGeometryNode, ESPMode::ThreadSafe>* Found = AssetPtr->GeometryIDToNodeMap.Find(InNodeID);
	if (Found != nullptr) return *Found;

	TSharedPtr<BFinalGeometryNode, ESPMode::ThreadSafe> NewNode = MakeShareable(new BFinalGeometryNode);
	NewNode->UniqueID = InNodeID;

	AssetPtr->GeometryIDToNodeMap.Add(InNodeID, NewNode);

	return NewNode;
}
TWeakPtr<BFinalMetadataNode, ESPMode::ThreadSafe> BFileAssetCreator::GetOrInsertMetadataNode(uint64 InNodeID)
{
	FScopeLock Lock(&MetadataIDToNodeMap_Mutex);
	TSharedPtr<BFinalMetadataNode, ESPMode::ThreadSafe>* Found = AssetPtr->MetadataIDToNodeMap.Find(InNodeID);
	if (Found != nullptr) return *Found;

	TSharedPtr<BFinalMetadataNode, ESPMode::ThreadSafe> NewNode = MakeShareable(new BFinalMetadataNode);
	NewNode->UniqueID = InNodeID;

	AssetPtr->MetadataIDToNodeMap.Add(InNodeID, NewNode);

	return NewNode;
}

void BFileAssetCreator::NewHierarchyNode(const BHierarchyNode& InNewNode)
{
	uint64 HNodeID = InNewNode.UniqueID;

	TSharedPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> HNode = GetOrInsertHierarchyNode(HNodeID).Pin();

	//There is no need to use ChildNodes; we are parsing upwards in the tree.

	//Parent h-node; assign self as a child
	if (InNewNode.ParentID == UNDEFINED_ID)
	{
		AssetPtr->RootNode = HNode;
	}
	else
	{
		uint64 ParentHNodeID = InNewNode.ParentID;

		TSharedPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> ParentHNode = GetOrInsertHierarchyNode(ParentHNodeID).Pin();
		HNode->Parent = ParentHNode;

		FScopeLock Lock(HierarchyID_ChildrenMutex_Map[ParentHNodeID].Get());
		ParentHNode->Children.Add(HNode);
	}

	uint64 MetadataNodeID = InNewNode.MetadataID;

	HNode->Metadata = GetOrInsertMetadataNode(MetadataNodeID);

	for (int32 i = 0; i < InNewNode.GeometryParts.Num(); i++)
	{
		auto& RefGPart = InNewNode.GeometryParts[i];
		uint64 GeometryNodeID = RefGPart.GeometryID;

		TSharedPtr<BFinalGeometryNode, ESPMode::ThreadSafe> GNode = GetOrInsertGeometryNode(GeometryNodeID).Pin();
		TSharedPtr<BFinalGeometryPart> UGPart = MakeShareable(new BFinalGeometryPart);

		UGPart->GeometryNode = GNode;

		UGPart->Transform = FTransform(
			FRotator(RefGPart.Rotation.X, RefGPart.Rotation.Y, RefGPart.Rotation.Z),
			FVector(RefGPart.Location.X, RefGPart.Location.Y, RefGPart.Location.Z),
			FVector(RefGPart.Scale.X, RefGPart.Scale.Y, RefGPart.Scale.Z));

		UGPart->Color = FColor(RefGPart.Color.R, RefGPart.Color.G, RefGPart.Color.B);

		HNode->Geometries.Add(UGPart);
	}

	{
		FScopeLock Lock(&HierarchyIDToNodeMap_Mutex);
		AssetPtr->HierarchyIDToNodeMap.Add(HNodeID, HNode);
	}
}

void BFileAssetCreator::NewMetadataNode(const BMetadataNode& InNewNode)
{
	TSharedPtr<BFinalMetadataNode, ESPMode::ThreadSafe> MNode = GetOrInsertMetadataNode(InNewNode.UniqueID).Pin();
	MNode->Metadata = InNewNode.Metadata;
}

void BFileAssetCreator::NewGeometryNode(const BGeometryNode& InNewNode)
{
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>();
	StaticMesh->LightingGuid = FGuid::NewGuid();
	StaticMesh->bAllowCPUAccess = true;
	StaticMesh->InitResources();

	static FColor DefaultVertexColor = FColor(255, 255, 255, 255);
	static FVector DefaultBuildScale3D = FVector(1.0f, 1.0f, 1.0f);

	for (int32 i = 0; i < InNewNode.LODs.Num(); i++)
	{
		auto& LODInfo = InNewNode.LODs[i];

		FStaticMeshSourceModel* SourceModel = &StaticMesh->AddSourceModel();

		//This part can normally be run on a separate thread; but since we're already in a task; it is both risky (thread-pool-deadlock due to wait() being called on running workers) and not good; performance-wise.
		{
			FRawMesh RawMesh;

			//Copy vertex data
			int32 VertexCount = LODInfo.VertexNormalTangentList.Num();
			RawMesh.VertexPositions.SetNumUninitialized(VertexCount);

			for (int32 j = 0; j < VertexCount; j++)
			{
				auto& Vertex = LODInfo.VertexNormalTangentList[j].Vertex;
				RawMesh.VertexPositions[j] = FVector(Vertex.X, Vertex.Y, Vertex.Z);
			}

			//Copy index data
			int32 IndexCount = LODInfo.Indexes.Num();
			check(IndexCount % 3 == 0);

			RawMesh.WedgeIndices.SetNumUninitialized(IndexCount);
			RawMesh.WedgeTangentX.SetNumUninitialized(IndexCount);
			RawMesh.WedgeTangentY.SetNumUninitialized(IndexCount);
			RawMesh.WedgeTangentZ.SetNumUninitialized(IndexCount);
			RawMesh.WedgeColors.SetNumUninitialized(IndexCount);
			RawMesh.WedgeTexCoords[0].SetNumZeroed(IndexCount);

			for (int32 j = 0; j < IndexCount; j++)
			{
				uint32 Indice = LODInfo.Indexes[j];

				check(Indice >= 0 || Indice < (uint32)VertexCount);

				RawMesh.WedgeIndices[j] = Indice;

				auto& VNT = LODInfo.VertexNormalTangentList[Indice];

				auto& XNormal = VNT.Normal;
				FVector Normal(XNormal.X, XNormal.Y, XNormal.Z);
				Normal.Normalize();

				auto& XTangent = VNT.Tangent;
				FVector Tangent(XTangent.X, XTangent.Y, XTangent.Z);
				Tangent.Normalize();

				FVector Bitangent = FVector::CrossProduct(Normal, Tangent);
				Bitangent.Normalize();

				//https://gamedev.stackexchange.com/a/51402
				RawMesh.WedgeTangentX[j] = Tangent;
				RawMesh.WedgeTangentY[j] = Bitangent;
				RawMesh.WedgeTangentZ[j] = Normal;

				RawMesh.WedgeColors[j] = DefaultVertexColor;
			}

			//Copy face data
			for (int32 j = 0; j < IndexCount / 3; j++)
			{
				RawMesh.FaceMaterialIndices.Add(0);
				RawMesh.FaceSmoothingMasks.Add(0xFFFFFFFF);
			}

			SourceModel->BuildSettings.bBuildAdjacencyBuffer = false;
			SourceModel->BuildSettings.bBuildReversedIndexBuffer = false;
			SourceModel->BuildSettings.bComputeWeightedNormals = false;
			SourceModel->BuildSettings.bGenerateDistanceFieldAsIfTwoSided = false;
			SourceModel->BuildSettings.bGenerateLightmapUVs = false;
			SourceModel->BuildSettings.bRecomputeNormals = false;
			SourceModel->BuildSettings.bRecomputeTangents = false;
			SourceModel->BuildSettings.bRemoveDegenerates = false;
			SourceModel->BuildSettings.bSupportFaceRemap = false; //Physical material mask
			SourceModel->BuildSettings.BuildScale3D = DefaultBuildScale3D;
			SourceModel->BuildSettings.bUseFullPrecisionUVs = false;
			SourceModel->BuildSettings.bUseHighPrecisionTangentBasis = false;
			SourceModel->BuildSettings.bUseMikkTSpace = false;
			SourceModel->BuildSettings.DistanceFieldResolutionScale = 0; //Do not generate

			SourceModel->RawMeshBulkData->SaveRawMesh(RawMesh);
		}
	}

	for (int32 j = 0; j < StaticMesh->StaticMaterials.Num(); j++)
	{
		FMeshSectionInfo Info = StaticMesh->GetSectionInfoMap().Get(0, j);
		Info.MaterialIndex = j;
		Info.bEnableCollision = false;
		StaticMesh->GetSectionInfoMap().Set(0, j, Info);
	}

	StaticMesh->CreateBodySetup();
	StaticMesh->BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;

	FEvent* GameThreadCompletedEvent = FGenericPlatformProcess::GetSynchEventFromPool();

	auto GameThreadTask = [StaticMesh, GameThreadCompletedEvent]()
	{
		BuildStaticMesh(StaticMesh, GameThreadCompletedEvent); //TODO: BatchBuild!
	};
	TaskQueuer(GameThreadTask);

	GameThreadCompletedEvent->Wait();
	FGenericPlatformProcess::ReturnSynchEventToPool(GameThreadCompletedEvent);

	TSharedPtr<BFinalGeometryNode, ESPMode::ThreadSafe> GNode = GetOrInsertGeometryNode(InNewNode.UniqueID).Pin();

	//Serialize static mesh render data
	BFileMeshSerialization::SerializeStaticMesh(StaticMesh, GNode->SerializedRenderData);
}

void BFileAssetCreator::BuildStaticMesh(UStaticMesh* StaticMesh, FEvent* DoneEvent)
{
	StaticMesh->Build(true);

	DoneEvent->Trigger();
}