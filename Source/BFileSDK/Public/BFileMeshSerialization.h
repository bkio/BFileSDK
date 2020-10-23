/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"

class BFILESDK_API BFileMeshSerialization
{
private:
	static void DeserializeToRenderData(const TArray<uint8>& SrcBuffer, class FStaticMeshRenderData* RenderData);

public:
	static void SerializeStaticMesh(class UStaticMesh* StaticMesh, TArray<uint8>& DestBuffer);
	static class UStaticMesh* DeserializeToStaticMesh(const TArray<uint8>& SrcBuffer);

	static void DeserializeToStaticMesh_ExecuteThreadablePart(class UStaticMesh* BlankStaticMesh, const TArray<uint8>& SrcBuffer);
	static void DeserializeToStaticMesh_ExecutePostThreadablePart(class UStaticMesh* StaticMesh);

	static const float DefaultLODScreenSizes[8/*MAX_STATIC_MESH_LODS*/];
};
