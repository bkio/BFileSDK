/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileMeshSerialization.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "MeshUtilities.h"
#include "StaticMeshResources.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Rendering/ColorVertexBuffer.h"
#include "StaticMeshVertexData.h"
#include "RawIndexBuffer.h"

void BFileMeshSerialization::SerializeStaticMesh(UStaticMesh* StaticMesh, TArray<uint8>& DestBuffer)
{
	check (StaticMesh);
	check (StaticMesh->RenderData);

	auto Serializer = FMemoryWriter(DestBuffer);
	auto& LODResources = StaticMesh->RenderData->LODResources;
	
	int32 LODCount = LODResources.Num();
	Serializer << LODCount;

	for (int32 i = 0; i < LODCount; i++)
	{
		auto& LOD = LODResources[i];

		//Sections part
		auto& Sections = LOD.Sections;

		int32 SectionsCount = Sections.Num();
		Serializer << SectionsCount;

		for (int32 j = 0; j < SectionsCount; j++)
		{
			auto& Section = Sections[j];

			int32 FirstIndex = Section.FirstIndex;
			Serializer << FirstIndex;

			int32 MaxVertexIndex = Section.MaxVertexIndex;
			Serializer << MaxVertexIndex;

			int32 MinVertexIndex = Section.MinVertexIndex;
			Serializer << MinVertexIndex;

			int32 NumTriangles = Section.NumTriangles;
			Serializer << NumTriangles;
		}

		//BuffersSize part
		uint32 BuffersSize = LOD.BuffersSize;
		Serializer << BuffersSize;

		//PositionVertexBuffer part
		uint32 PositionVertexBuffer_NumVertices = LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
		Serializer << PositionVertexBuffer_NumVertices;

		uint8* PositionVertexBuffer_VertexData = (uint8*)LOD.VertexBuffers.PositionVertexBuffer.GetVertexData();
		{
			TArray<uint8> TmpBuffer;
			TmpBuffer.AddUninitialized(12/*VectorSize*/ * PositionVertexBuffer_NumVertices);
			FMemory::Memcpy(TmpBuffer.GetData(), PositionVertexBuffer_VertexData, TmpBuffer.Num());
			Serializer << TmpBuffer;
		}
		
		//StaticMeshVertexBuffer part
		uint32 StaticMeshVertexBuffer_TangentSize = LOD.VertexBuffers.StaticMeshVertexBuffer.GetTangentSize();
		Serializer << StaticMeshVertexBuffer_TangentSize;
		
		uint8* StaticMeshVertexBuffer_TangentsData = (uint8*)LOD.VertexBuffers.StaticMeshVertexBuffer.GetTangentData();
		{
			TArray<uint8> TmpBuffer;
			TmpBuffer.AddUninitialized(StaticMeshVertexBuffer_TangentSize);
			FMemory::Memcpy(TmpBuffer.GetData(), StaticMeshVertexBuffer_TangentsData, TmpBuffer.Num());
			Serializer << TmpBuffer;
		}

		uint32 StaticMeshVertexBuffer_TexCoordSize = LOD.VertexBuffers.StaticMeshVertexBuffer.GetTexCoordSize();
		Serializer << StaticMeshVertexBuffer_TexCoordSize;

		uint8* StaticMeshVertexBuffer_TexcoordData = (uint8*)LOD.VertexBuffers.StaticMeshVertexBuffer.GetTexCoordData();
		{
			TArray<uint8> TmpBuffer;
			TmpBuffer.AddUninitialized(StaticMeshVertexBuffer_TexCoordSize);
			FMemory::Memcpy(TmpBuffer.GetData(), StaticMeshVertexBuffer_TexcoordData, TmpBuffer.Num());
			Serializer << TmpBuffer;
		}

		//ColorVertexBuffer part - not used

		//IndexBuffer part
		TArray<uint32> IndexBuffer;
		LOD.IndexBuffer.GetCopy(IndexBuffer);

		Serializer << IndexBuffer;
	}

	Serializer << StaticMesh->RenderData->Bounds;
}

void BFileMeshSerialization::DeserializeToRenderData(const TArray<uint8>& SrcBuffer, FStaticMeshRenderData* RenderData)
{
	
	auto Deserializer = FMemoryReader(SrcBuffer);

	int32 LODCount;
	Deserializer << LODCount;

	RenderData->LODResources.Empty();
	RenderData->AllocateLODResources(LODCount);
	RenderData->LODVertexFactories.Empty(LODCount);
	RenderData->Bounds = FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.f);

	for (int32 i = 0; i < LODCount; i++)
	{
		auto& LODResource = RenderData->LODResources[i];

		LODResource.bBuffersInlined = true;
		LODResource.bHasAdjacencyInfo = false;
		LODResource.bHasDepthOnlyIndices = false;
		LODResource.bHasRayTracingGeometry = false;
		LODResource.bHasReversedDepthOnlyIndices = false;
		LODResource.bHasReversedIndices = false;
		LODResource.bHasWireframeIndices = false;
		LODResource.bIsOptionalLOD = false;

		//Sections part
		int32 SectionsCount;
		Deserializer << SectionsCount;

		LODResource.Sections.Empty();
		LODResource.Sections.AddDefaulted(SectionsCount);

		for (int32 j = 0; j < SectionsCount; j++)
		{
			auto& Section = LODResource.Sections[j];

			Section.bCastShadow = true;
			Section.bEnableCollision = false;
			Section.bForceOpaque = false;

			Section.MaterialIndex = 0;

			Deserializer << Section.FirstIndex;
			Deserializer << Section.MaxVertexIndex;
			Deserializer << Section.MinVertexIndex;
			Deserializer << Section.NumTriangles;
		}

		new(RenderData->LODVertexFactories) FStaticMeshVertexFactories(ERHIFeatureLevel::SM5);

		//MaxDeviation part
		LODResource.MaxDeviation = 0.0f;

		//BuffersSize part
		Deserializer << LODResource.BuffersSize;
		
		//Note: Deserialize empties/resizes the array; no need to call AddUninitialized.

		//PositionVertexBuffer part		
		uint32 PositionVertexBuffer_NumVertices;
		Deserializer << PositionVertexBuffer_NumVertices;

		TArray<uint8> PositionVertexBuffer_VertexData;
		Deserializer << PositionVertexBuffer_VertexData;

		LODResource.VertexBuffers.PositionVertexBuffer.CleanUp();
		LODResource.VertexBuffers.PositionVertexBuffer.Init(PositionVertexBuffer_NumVertices, false/*bNeedsCPUAccess*/);
		FMemory::Memcpy(
			LODResource.VertexBuffers.PositionVertexBuffer.GetVertexData(), 
			PositionVertexBuffer_VertexData.GetData(), 
			PositionVertexBuffer_NumVertices * 12/*VectorSize*/);

		//StaticMeshVertexBuffer part

		uint32 StaticMeshVertexBuffer_NumVertices;
		Deserializer << StaticMeshVertexBuffer_NumVertices;

		TArray<uint8> StaticMeshVertexBuffer_TangentsData; 
		Deserializer << StaticMeshVertexBuffer_TangentsData;

		uint32 StaticMeshVertexTexBuffer_NumVertices;
		Deserializer << StaticMeshVertexTexBuffer_NumVertices;

		TArray<uint8> StaticMeshVertexBuffer_TexcoordData;
		Deserializer << StaticMeshVertexBuffer_TexcoordData;

		LODResource.VertexBuffers.StaticMeshVertexBuffer.CleanUp();
		LODResource.VertexBuffers.StaticMeshVertexBuffer.Init(StaticMeshVertexBuffer_NumVertices, 1/*InNumTexCoords*/, false/*bNeedsCPUAccess*/);
		FMemory::Memcpy(
			LODResource.VertexBuffers.StaticMeshVertexBuffer.GetTangentData(),
			StaticMeshVertexBuffer_TangentsData.GetData(),
			StaticMeshVertexBuffer_NumVertices);
		FMemory::Memcpy(
			LODResource.VertexBuffers.StaticMeshVertexBuffer.GetTexCoordData(),
			StaticMeshVertexBuffer_TexcoordData.GetData(),
			StaticMeshVertexBuffer_NumVertices);

		//ColorVertexBuffer part - not needed
		LODResource.VertexBuffers.ColorVertexBuffer.CleanUp();

		//IndexBuffer part
		TArray<uint32> IndexBuffer;
		Deserializer << IndexBuffer;

		LODResource.IndexBuffer.SetIndices(IndexBuffer, EIndexBufferStride::Force32Bit);
	}

	Deserializer << RenderData->Bounds;

	RenderData->bLODsShareStaticLighting = false;
}

//Not in use anymore
UStaticMesh* BFileMeshSerialization::DeserializeToStaticMesh(const TArray<uint8>& SrcBuffer)
{
	auto StaticMesh = NewObject<UStaticMesh>();

	DeserializeToStaticMesh_ExecuteThreadablePart(StaticMesh, SrcBuffer);
	DeserializeToStaticMesh_ExecutePostThreadablePart(StaticMesh);

	return StaticMesh;
}
void BFileMeshSerialization::DeserializeToStaticMesh_ExecuteThreadablePart(class UStaticMesh* BlankStaticMesh, const TArray<uint8>& SrcBuffer)
{
	BlankStaticMesh->bAllowCPUAccess = false;

	BlankStaticMesh->RenderData = TUniquePtr<FStaticMeshRenderData>(new FStaticMeshRenderData());
	DeserializeToRenderData(SrcBuffer, BlankStaticMesh->RenderData.Get());
}
void BFileMeshSerialization::DeserializeToStaticMesh_ExecutePostThreadablePart(UStaticMesh* StaticMesh)
{
	StaticMesh->InitResources();
	StaticMesh->LightingGuid = FGuid::NewGuid();
	StaticMesh->CalculateExtendedBounds();
	StaticMesh->CreateBodySetup();
	StaticMesh->BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;

	StaticMesh->bAutoComputeLODScreenSize = false;
	for (int32 i = 0; i < MAX_STATIC_MESH_LODS; i++)
	{
		StaticMesh->RenderData->ScreenSize[i] = DefaultLODScreenSizes[i];
	}
}

const float BFileMeshSerialization::DefaultLODScreenSizes[8/*MAX_STATIC_MESH_LODS*/] =
{
	1.0f,
	0.1f,
	0.025f,

	0.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f
};