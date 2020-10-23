/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileCommonTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#define XCopyFromBytes(HEAD, SRC, BUFF) if ((sizeof(SRC) + (HEAD - BUFF.GetData())) <= BUFF.Num()) { FMemory::Memcpy(&SRC, HEAD, sizeof(SRC)); HEAD += sizeof(SRC); } else return false;

bool BNode::FromBytes(uint32& NodeSize, const uint8* InHead, const TArray<uint8>& Buffer)
{
	const uint8* Head = InHead;
	XCopyFromBytes(Head, UniqueID, Buffer);
	NodeSize = Head - InHead;
	return true;
}

bool BMetadataNode::FromBytes(uint32& NodeSize, const uint8* InHead, const TArray<uint8>& Buffer)
{
	const uint8* Head = InHead;

	uint32 BaseNodeSize;
	if (!BNode::FromBytes(BaseNodeSize, Head, Buffer)) return false;
	Head += BaseNodeSize;

	int32 MetadataSize;
	XCopyFromBytes(Head, MetadataSize, Buffer);

	TArray<uint8> MetadataContent;
	MetadataContent.AddUninitialized(MetadataSize);
	FMemory::Memcpy(MetadataContent.GetData(), Head, MetadataSize);
	Head += MetadataSize;

	FString MetadataString = ANSI_TO_TCHAR((ANSICHAR*)MetadataContent.GetData());

	//In case there are additional characters produced by decoding at the beginning or the end; trim them.
	int32 IndexOfFirstExpectedChar = MetadataString.Find("{\"", ESearchCase::IgnoreCase, ESearchDir::FromStart);
	if (IndexOfFirstExpectedChar > 0)
	{
		MetadataString = MetadataString.Mid(IndexOfFirstExpectedChar);
	}
	int32 IndexOfLastExpectedChar = MetadataString.Find("\"}", ESearchCase::IgnoreCase, ESearchDir::FromStart);
	if (IndexOfLastExpectedChar > 0 && IndexOfLastExpectedChar < (MetadataString.Len() - 2)) /*sizeof("\"}")*/
	{
		MetadataString = MetadataString.Mid(0, IndexOfLastExpectedChar + 2);
	}

	TSharedPtr<FJsonObject> TmpObj;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MetadataString), TmpObj))
	{
		UE_LOG(LogTemp, Error, TEXT("BMetadataNode::FromBytes: Deserialization failed: %s"), *MetadataString);
		Metadata = MakeShareable(new FJsonObject);
	}
	else
	{
		Metadata = MakeShareable(new FJsonObject(*TmpObj.Get()));
	}

	NodeSize = Head - InHead;
	return true;
}

bool BHierarchyNode::FromBytes(uint32& NodeSize, const uint8* InHead, const TArray<uint8>& Buffer)
{
	const uint8* Head = InHead;
	
	uint32 BaseNodeSize;
	if (!BNode::FromBytes(BaseNodeSize, Head, Buffer)) return false;
	Head += BaseNodeSize;

	XCopyFromBytes(Head, ParentID, Buffer);
	XCopyFromBytes(Head, MetadataID, Buffer);
	
	int32 GeometryPartsSize;
	XCopyFromBytes(Head, GeometryPartsSize, Buffer);
	GeometryParts.SetNum(GeometryPartsSize);
	for (int32 i = 0; i < GeometryPartsSize; i++)
	{
		XCopyFromBytes(Head, GeometryParts[i].GeometryID, Buffer);
		XCopyFromBytes(Head, GeometryParts[i].Location.X, Buffer);
		XCopyFromBytes(Head, GeometryParts[i].Location.Y, Buffer);
		XCopyFromBytes(Head, GeometryParts[i].Location.Z, Buffer);

		XCopyFromBytes(Head, GeometryParts[i].Rotation.X, Buffer);
		XCopyFromBytes(Head, GeometryParts[i].Rotation.Y, Buffer);
		XCopyFromBytes(Head, GeometryParts[i].Rotation.Z, Buffer);

		XCopyFromBytes(Head, GeometryParts[i].Scale.X, Buffer);
		XCopyFromBytes(Head, GeometryParts[i].Scale.Y, Buffer);
		XCopyFromBytes(Head, GeometryParts[i].Scale.Z, Buffer);

		XCopyFromBytes(Head, GeometryParts[i].Color.R, Buffer);
		XCopyFromBytes(Head, GeometryParts[i].Color.G, Buffer);
		XCopyFromBytes(Head, GeometryParts[i].Color.B, Buffer);
	}
	int32 ChildNodesSize;
	XCopyFromBytes(Head, ChildNodesSize, Buffer);
	ChildNodes.SetNum(ChildNodesSize);
	for (int32 i = 0; i < ChildNodesSize; i++)
	{
		XCopyFromBytes(Head, ChildNodes[i], Buffer);
	}

	NodeSize = Head - InHead;
	return true;
}

bool BGeometryNode::FromBytes(uint32& NodeSize, const uint8* InHead, const TArray<uint8>& Buffer)
{
	const uint8* Head = InHead;

	uint32 BaseNodeSize;
	if (!BNode::FromBytes(BaseNodeSize, Head, Buffer)) return false;
	Head += BaseNodeSize;

	int8 LODCount;
	XCopyFromBytes(Head, LODCount, Buffer);
	LODs.SetNum(LODCount);

	for (int8 i = 0; i < LODCount; i++)
	{
		int32 VNTCount;
		XCopyFromBytes(Head, VNTCount, Buffer);
		LODs[i].VertexNormalTangentList.SetNum(VNTCount);
		for (int32 j = 0; j < VNTCount; j++)
		{
			XCopyFromBytes(Head, LODs[i].VertexNormalTangentList[j].Vertex.X, Buffer);
			XCopyFromBytes(Head, LODs[i].VertexNormalTangentList[j].Vertex.Y, Buffer);
			XCopyFromBytes(Head, LODs[i].VertexNormalTangentList[j].Vertex.Z, Buffer);

			XCopyFromBytes(Head, LODs[i].VertexNormalTangentList[j].Normal.X, Buffer);
			XCopyFromBytes(Head, LODs[i].VertexNormalTangentList[j].Normal.Y, Buffer);
			XCopyFromBytes(Head, LODs[i].VertexNormalTangentList[j].Normal.Z, Buffer);

			XCopyFromBytes(Head, LODs[i].VertexNormalTangentList[j].Tangent.X, Buffer);
			XCopyFromBytes(Head, LODs[i].VertexNormalTangentList[j].Tangent.Y, Buffer);
			XCopyFromBytes(Head, LODs[i].VertexNormalTangentList[j].Tangent.Z, Buffer);
		}

		int32 IndexedTrianglesCount;
		XCopyFromBytes(Head, IndexedTrianglesCount, Buffer);
		LODs[i].Indexes.SetNumUninitialized(IndexedTrianglesCount);
		for (int32 j = 0; j < IndexedTrianglesCount; j++)
		{
			XCopyFromBytes(Head, LODs[i].Indexes[j], Buffer);
		}
	}

	NodeSize = Head - InHead;
	return true;
}