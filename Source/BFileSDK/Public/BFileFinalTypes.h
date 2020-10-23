/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"

class FArchiveSaveCompressedProxy;
class FArchiveLoadCompressedProxy;

enum BFILESDK_API EBFileOutputFormat : uint8
{
	HGM = 1, //Hierarchy-geometry-metadata combined
	HG = 2, //Hierarchy-geometry combined
	H = 3, //Hierarchy-only
	Gs = 4	 //One file per geometry node	
};

//Writing to an array; to stream or directly to array?
struct BFILESDK_API FBFileOutputBufferAlternative
{
	enum EBFileFactoryOutputBufferAlternative : uint8
	{
		Stream = 1,
		Array = 2
	};
	EBFileFactoryOutputBufferAlternative Alternative;

	std::ostream* StreamPtr;
	TFunction<void()> StreamDoneWritingCallback;

	TArray<uint8>* ArrayPtr;

	FBFileOutputBufferAlternative(std::ostream* WithStream, TFunction<void()> WithStreamDoneWritingCallback)
	{
		Alternative = EBFileFactoryOutputBufferAlternative::Stream;
		StreamPtr = WithStream;
		StreamDoneWritingCallback = WithStreamDoneWritingCallback;
	}
	FBFileOutputBufferAlternative(TArray<uint8>* WithTArray)
	{
		Alternative = EBFileFactoryOutputBufferAlternative::Array;
		ArrayPtr = WithTArray;
	}
	FBFileOutputBufferAlternative() 
	{
		Alternative = EBFileFactoryOutputBufferAlternative::Stream;
		StreamPtr = nullptr;
		StreamDoneWritingCallback = nullptr;
		ArrayPtr = nullptr;
	}
};

class BFILESDK_API BFinalNode
{
public:
	int64 UniqueID;
};

class BFILESDK_API BFinalMetadataNode : public BFinalNode
{
public:
	TSharedPtr<class FJsonObject, ESPMode::ThreadSafe> Metadata;
};

class BFILESDK_API BFinalGeometryNode : public BFinalNode
{
public:
	TArray<uint8> SerializedRenderData;
};

class BFILESDK_API BFinalGeometryPart
{
public:
	TWeakPtr<BFinalGeometryNode, ESPMode::ThreadSafe> GeometryNode;

	FTransform Transform;

	FColor Color;

	//Only available render-time
	int32 InstanceIndex;
};

class BFILESDK_API BFinalHiearchyNode : public BFinalNode
{
public:
	TWeakPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> Parent;

	TArray<TWeakPtr<BFinalHiearchyNode, ESPMode::ThreadSafe>> Children;

	TArray<TSharedPtr<BFinalGeometryPart>> Geometries;

	TWeakPtr<BFinalMetadataNode, ESPMode::ThreadSafe> Metadata;
};

class BFILESDK_API BFinalAssetContent
{
public:
	TMap<int64, TSharedPtr<BFinalHiearchyNode, ESPMode::ThreadSafe>> HierarchyIDToNodeMap;

	TMap<int64, TSharedPtr<BFinalGeometryNode, ESPMode::ThreadSafe>> GeometryIDToNodeMap;

	TMap<int64, TSharedPtr<BFinalMetadataNode, ESPMode::ThreadSafe>> MetadataIDToNodeMap;

	TWeakPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> RootNode;

	bool XSerialize(EBFileOutputFormat OutputFormat, TFunction<FBFileOutputBufferAlternative(int64)> OutputBuffer);
	void XDeserialize(const TArray<uint8>& SrcBuffer);

private:
	bool XSerialize_Gs(TFunction<FBFileOutputBufferAlternative(int64)> OutputBuffer);
	bool XSerialize_Others(EBFileOutputFormat OutputFormat, TFunction<FBFileOutputBufferAlternative(int64)> OutputBuffer);

	void XDeserialize_Gs(FArchiveLoadCompressedProxy& Deserializer);
	void XDeserialize_Others(EBFileOutputFormat OutputFormat, FArchiveLoadCompressedProxy& Deserializer);

	static void XSerialize_Recursive(FArchiveSaveCompressedProxy& Serializer, TWeakPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> Node);
	TWeakPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> XDeserialize_Recursive(FArchiveLoadCompressedProxy& Deserializer);
};