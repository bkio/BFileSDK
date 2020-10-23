/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileFinalTypes.h"
#include "BFileCommonTypes.h"
#include "Serialization/ArchiveSaveCompressedProxy.h"
#include "Serialization/ArchiveLoadCompressedProxy.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "BLambdaRunnable.h"

bool BFinalAssetContent::XSerialize(EBFileOutputFormat OutputFormat, TFunction<FBFileOutputBufferAlternative(int64)> OutputBuffer)
{
	if (RootNode == nullptr) return false;

	if (OutputFormat != EBFileOutputFormat::Gs)
	{
		//One big file contains bunch
		return XSerialize_Others(OutputFormat, OutputBuffer);
	}

	//Geometry nodes as separate files
	return XSerialize_Gs(OutputBuffer);
}

bool BFinalAssetContent::XSerialize_Gs(TFunction<FBFileOutputBufferAlternative(int64)> OutputBuffer)
{
	if (GeometryIDToNodeMap.Num() == 0) return false;

	bool* bSucceedPtr = new bool(true);

	FThreadSafeCounter* UncompletedTasksCount = new FThreadSafeCounter(GeometryIDToNodeMap.Num());
	FEvent* CompletedEvent = FGenericPlatformProcess::GetSynchEventFromPool();

	for (auto& GPair : GeometryIDToNodeMap)
	{
		const int64 ConstGUniqueID = GPair.Key;
		BFinalGeometryNode* GNodePtr = GPair.Value.Get();

		FBLambdaRunnable::RunLambdaOnBackgroundThreadPool([ConstGUniqueID, GNodePtr, OutputBuffer, UncompletedTasksCount, CompletedEvent, bSucceedPtr]()
			{
				if (*bSucceedPtr == false)
				{
					if (UncompletedTasksCount->Decrement() == 0) CompletedEvent->Trigger();
					return;
				}

				int64 GUniqueID = ConstGUniqueID;

				TArray<uint8>* DestBufferPtr;
				bool bDeleteDestBufferPtr = false;

				std::ostream* WriteToStream = nullptr;
				TFunction<void()> StreamDoneWritingCallback = nullptr;
				bool bWriteBufferToStream = false;

				FBFileOutputBufferAlternative GeneratedDestBuffer = OutputBuffer(GUniqueID);

				if (GeneratedDestBuffer.Alternative == FBFileOutputBufferAlternative::Array)
				{
					DestBufferPtr = GeneratedDestBuffer.ArrayPtr;
					if (DestBufferPtr == nullptr)
					{
						*bSucceedPtr = false;
						if (UncompletedTasksCount->Decrement() == 0) CompletedEvent->Trigger();
						return;
					}
				}
				else
				{
					StreamDoneWritingCallback = GeneratedDestBuffer.StreamDoneWritingCallback;
					WriteToStream = GeneratedDestBuffer.StreamPtr;
					if (WriteToStream == nullptr)
					{
						*bSucceedPtr = false;
						if (UncompletedTasksCount->Decrement() == 0) CompletedEvent->Trigger();
						return;
					}

					DestBufferPtr = new TArray<uint8>();
					bDeleteDestBufferPtr = true;
					bWriteBufferToStream = true;
				}

				DestBufferPtr->Empty();

				FArchiveSaveCompressedProxy Serializer = FArchiveSaveCompressedProxy(*DestBufferPtr, NAME_Zlib, COMPRESS_BiasMemory);
				Serializer.SetFilterEditorOnly(true);

				//Serialization starts
				uint8 OutputFormatAsByte = (uint8)EBFileOutputFormat::Gs;
				Serializer << OutputFormatAsByte;

				Serializer << GUniqueID;

				auto& GSerialized = GNodePtr->SerializedRenderData;
				Serializer << GSerialized;
				//Serialization ends

				Serializer.Flush();

				if (bWriteBufferToStream)
				{
					WriteToStream->write((char*)DestBufferPtr->GetData(), DestBufferPtr->Num());
					if (StreamDoneWritingCallback)
					{
						StreamDoneWritingCallback();
					}
				}
				if (bDeleteDestBufferPtr)
				{
					delete DestBufferPtr;
				}

				if (UncompletedTasksCount->Decrement() == 0) CompletedEvent->Trigger();
			});
	}

	CompletedEvent->Wait();
	FGenericPlatformProcess::ReturnSynchEventToPool(CompletedEvent);
	delete UncompletedTasksCount;

	bool bSucceed = *bSucceedPtr;
	delete bSucceedPtr;
	return bSucceed;
}

bool BFinalAssetContent::XSerialize_Others(EBFileOutputFormat OutputFormat, TFunction<FBFileOutputBufferAlternative(int64)> OutputBuffer)
{
	static int64 IgnoreFileNodeID = 0; //Only meaningful for Gs

	TArray<uint8>* DestBufferPtr;
	bool bDeleteDestBufferPtr = false;

	std::ostream* WriteToStream = nullptr;
	TFunction<void()> StreamDoneWritingCallback = nullptr;
	bool bWriteBufferToStream = false;

	FBFileOutputBufferAlternative GeneratedDestBuffer = OutputBuffer(IgnoreFileNodeID);

	if (GeneratedDestBuffer.Alternative == FBFileOutputBufferAlternative::Array)
	{
		DestBufferPtr = GeneratedDestBuffer.ArrayPtr;
		if (DestBufferPtr == nullptr)
		{
			return false;
		}
	}
	else
	{
		StreamDoneWritingCallback = GeneratedDestBuffer.StreamDoneWritingCallback;
		WriteToStream = GeneratedDestBuffer.StreamPtr;
		if (WriteToStream == nullptr)
		{
			return false;
		}

		DestBufferPtr = new TArray<uint8>();
		bDeleteDestBufferPtr = true;
		bWriteBufferToStream = true;
	}

	DestBufferPtr->Empty();

	FArchiveSaveCompressedProxy Serializer = FArchiveSaveCompressedProxy(*DestBufferPtr, NAME_Zlib, COMPRESS_BiasMemory);
	Serializer.SetFilterEditorOnly(true);

	//Serialization starts
	uint8 OutputFormatAsByte = (uint8)OutputFormat;
	Serializer << OutputFormatAsByte;

	if (OutputFormat == EBFileOutputFormat::HGM || OutputFormat == EBFileOutputFormat::HG)
	{
		int32 NumGeometryNodes = GeometryIDToNodeMap.Num();
		Serializer << NumGeometryNodes;

		for (auto& GPair : GeometryIDToNodeMap)
		{
			int64 GUniqueID = GPair.Key;
			Serializer << GUniqueID;

			auto& GSerialized = GPair.Value->SerializedRenderData;
			Serializer << GSerialized;
		}
	}
	
	if (OutputFormat == EBFileOutputFormat::HGM)
	{
		int32 NumMetadataNodes = MetadataIDToNodeMap.Num();
		Serializer << NumMetadataNodes;

		for (auto& MPair : MetadataIDToNodeMap)
		{
			int64 MUniqueID = MPair.Key;
			Serializer << MUniqueID;

			FString MetadataString;
			if (MPair.Value.IsValid() && MPair.Value->Metadata.IsValid())
			{
				TSharedPtr<FJsonObject> TmpObj = MakeShareable(new FJsonObject(*MPair.Value->Metadata.Get()));
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MetadataString);
				FJsonSerializer::Serialize(TmpObj.ToSharedRef(), Writer);
			}
			else
			{
				MetadataString = "{}";
			}
			Serializer << MetadataString;
		}
	}
	
	XSerialize_Recursive(Serializer, RootNode);
	//Serialization ends

	Serializer.Flush();

	if (bWriteBufferToStream)
	{
		WriteToStream->write((char*)DestBufferPtr->GetData(), DestBufferPtr->Num());
		if (StreamDoneWritingCallback)
		{
			StreamDoneWritingCallback();
		}
	}
	if (bDeleteDestBufferPtr)
	{
		delete DestBufferPtr;
	}
	return true;
}

void BFinalAssetContent::XSerialize_Recursive(FArchiveSaveCompressedProxy& Serializer, TWeakPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> Node)
{
	auto Pinned = Node.Pin();

	int64 HUniqueID = Pinned->UniqueID;
	Serializer << HUniqueID;

	int64 ParentHUniqueID = Pinned->Parent.IsValid() ? Pinned->Parent.Pin()->UniqueID : UNDEFINED_ID;
	Serializer << ParentHUniqueID;

	int64 MUniqueID = Pinned->Metadata.Pin()->UniqueID;
	Serializer << MUniqueID;

	int32 NumGeometries = Pinned->Geometries.Num();
	Serializer << NumGeometries;

	for (int32 i = 0; i < NumGeometries; i++)
	{
		auto GPart = Pinned->Geometries[i];

		int64 GUniqueID = GPart->GeometryNode.Pin()->UniqueID;
		Serializer << GUniqueID;

		auto& Transform = GPart->Transform;
		Serializer << Transform;

		auto& Color = GPart->Color;
		Serializer << Color;
	}

	int32 NumChildren = Pinned->Children.Num();
	Serializer << NumChildren;

	for (int32 i = 0; i < NumChildren; i++)
	{
		XSerialize_Recursive(Serializer, Pinned->Children[i]);
	}
}

void BFinalAssetContent::XDeserialize(const TArray<uint8>& SrcBuffer)
{
	FArchiveLoadCompressedProxy Deserializer = FArchiveLoadCompressedProxy(SrcBuffer, NAME_Zlib);
	Deserializer.SetFilterEditorOnly(true);

	//Deserialization starts
	uint8 OutputFormatAsByte;
	Deserializer << OutputFormatAsByte;
	EBFileOutputFormat OutputFormat = (EBFileOutputFormat)OutputFormatAsByte;
	
	if (OutputFormat != EBFileOutputFormat::Gs)
	{
		XDeserialize_Others(OutputFormat, Deserializer);
	}
	else
	{
		XDeserialize_Gs(Deserializer);
	}
	//Deserialization ends
}

void BFinalAssetContent::XDeserialize_Gs(FArchiveLoadCompressedProxy& Deserializer)
{
	TSharedPtr<BFinalGeometryNode, ESPMode::ThreadSafe> NewGNode = MakeShareable(new BFinalGeometryNode);

	Deserializer << NewGNode->UniqueID;

	Deserializer << NewGNode->SerializedRenderData;

	GeometryIDToNodeMap.Add(NewGNode->UniqueID, NewGNode);
}

void BFinalAssetContent::XDeserialize_Others(EBFileOutputFormat OutputFormat, FArchiveLoadCompressedProxy& Deserializer)
{
	if (OutputFormat == EBFileOutputFormat::HGM || OutputFormat == EBFileOutputFormat::HG)
	{
		int32 NumGeometryNodes;
		Deserializer << NumGeometryNodes;

		GeometryIDToNodeMap.Empty(NumGeometryNodes);
		for (int32 i = 0; i < NumGeometryNodes; i++)
		{
			TSharedPtr<BFinalGeometryNode, ESPMode::ThreadSafe> NewGNode = MakeShareable(new BFinalGeometryNode);

			Deserializer << NewGNode->UniqueID;

			Deserializer << NewGNode->SerializedRenderData;

			GeometryIDToNodeMap.Add(NewGNode->UniqueID, NewGNode);
		}
	}

	if (OutputFormat == EBFileOutputFormat::HGM)
	{
		int32 NumMetadataNodes;
		Deserializer << NumMetadataNodes;

		MetadataIDToNodeMap.Empty(NumMetadataNodes);
		for (int32 i = 0; i < NumMetadataNodes; i++)
		{
			TSharedPtr<BFinalMetadataNode, ESPMode::ThreadSafe> NewMNode = MakeShareable(new BFinalMetadataNode);

			Deserializer << NewMNode->UniqueID;

			FString MetadataString;
			Deserializer << MetadataString;

			TSharedPtr<FJsonObject> TmpObj;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MetadataString), TmpObj))
			{
				UE_LOG(LogTemp, Error, TEXT("BFinalAssetContent::XDeserialize_Others: Deserialization failed: %s"), *MetadataString);
				NewMNode->Metadata = MakeShareable(new FJsonObject);
			}
			else
			{
				NewMNode->Metadata = MakeShareable(new FJsonObject(*TmpObj.Get()));
			}

			MetadataIDToNodeMap.Add(NewMNode->UniqueID, NewMNode);
		}
	}

	RootNode = XDeserialize_Recursive(Deserializer);
}

TWeakPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> BFinalAssetContent::XDeserialize_Recursive(FArchiveLoadCompressedProxy& Deserializer)
{
	TSharedPtr<BFinalHiearchyNode, ESPMode::ThreadSafe> NewHNode = MakeShareable(new BFinalHiearchyNode);

	Deserializer << NewHNode->UniqueID;
	HierarchyIDToNodeMap.Add(NewHNode->UniqueID, NewHNode);

	int64 ParentHUniqueID;
	Deserializer << ParentHUniqueID;
	NewHNode->Parent = ParentHUniqueID == UNDEFINED_ID ? nullptr : HierarchyIDToNodeMap[ParentHUniqueID];

	int64 MUniqueID;
	Deserializer << MUniqueID;
	NewHNode->Metadata = MetadataIDToNodeMap[MUniqueID];

	int32 NumGeometries;
	Deserializer << NumGeometries;
	NewHNode->Geometries.SetNum/*Uninitialized-crashes*/(NumGeometries);

	for (int32 i = 0; i < NumGeometries; i++)
	{
		TSharedPtr<BFinalGeometryPart> NewGPart = MakeShareable(new BFinalGeometryPart);
		NewHNode->Geometries[i] = NewGPart;

		int64 GUniqueID;
		Deserializer << GUniqueID;
		NewGPart->GeometryNode = GeometryIDToNodeMap[GUniqueID];

		Deserializer << NewGPart->Transform;

		Deserializer << NewGPart->Color;
	}

	int32 NumChildren;
	Deserializer << NumChildren;
	NewHNode->Children.SetNum/*Uninitialized-crashes*/(NumChildren);

	for (int32 i = 0; i < NumChildren; i++)
	{
		NewHNode->Children[i] = XDeserialize_Recursive(Deserializer);
	}

	return NewHNode;
}