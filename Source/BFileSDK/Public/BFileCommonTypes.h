/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#define UNDEFINED_ID 0xFFFFFFFF00000000

enum BFILESDK_API EBNodeType : uint8
{
	EBNodeType_Hierarchy = 0,
	EBNodeType_Geometry = 1,
	EBNodeType_Metadata = 2
};

struct BFILESDK_API BVector
{
	BVector() : X(0), Y(0), Z(0) {}
	BVector(const BVector& Other) : X(Other.X), Y(Other.Y), Z(Other.Z) {}
	BVector(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}

	float X;
	float Y;
	float Z;
};

struct BFILESDK_API BColor
{
	BColor() : R(0), G(0), B(0) {}
	BColor(const BColor& Other) : R(Other.R), G(Other.G), B(Other.B) {}
	BColor(uint8 InR, uint8 InG, uint8 InB) : R(InR), G(InG), B(InB) {}

	uint8 R;
	uint8 G;
	uint8 B;
};

class BFILESDK_API BNode
{
public:
	uint64 UniqueID = UNDEFINED_ID;

	BNode() {}
	virtual ~BNode() {}

	virtual bool FromBytes(uint32& NodeSize, const uint8* InHead, const TArray<uint8>& Buffer);
};

class BFILESDK_API BMetadataNode : public BNode
{
public:
	TSharedPtr<class FJsonObject, ESPMode::ThreadSafe> Metadata;

	virtual bool FromBytes(uint32& NodeSize, const uint8* InHead, const TArray<uint8>& Buffer) override;
};

class BFILESDK_API BHierarchyNode : public BNode
{
public:
	uint64 ParentID = UNDEFINED_ID;
	uint64 MetadataID = UNDEFINED_ID;

	struct GeometryPart
	{
		uint64 GeometryID = UNDEFINED_ID;

		BVector Location = BVector(0.0f, 0.0f, 0.0f);
		BVector Rotation = BVector(0.0f, 0.0f, 0.0f);
		BVector Scale = BVector(1.0f, 1.0f, 1.0f);

		BColor Color;
	};

	TArray<BHierarchyNode::GeometryPart> GeometryParts;

	TArray<uint64> ChildNodes;

	virtual bool FromBytes(uint32& NodeSize, const uint8* InHead, const TArray<uint8>& Buffer) override;
};

class BFILESDK_API BGeometryNode : public BNode
{
public:
	struct BLOD
	{
		struct BVertexNormalTangent
		{
			BVector Vertex;
			BVector Normal;
			BVector Tangent;
		};
		TArray<BVertexNormalTangent> VertexNormalTangentList;

		TArray<uint32> Indexes;
	};

	TArray<BLOD> LODs;

	virtual bool FromBytes(uint32& NodeSize, const uint8* InHead, const TArray<uint8>& Buffer) override;
};