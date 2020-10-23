/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "BFileHeader.h"
#include <istream>

class BFILESDK_API BFileReader : public BFileHeader
{
public:
	BFileReader(EBNodeType InFileType)
	{
		FileType = InFileType;
	}

	void ReadFromStream(
		std::istream* InStream, 
		EBFileCompressionState InCompressionState, 
		TFunction<void(uint32)> OnFileSDKVersionRead_Callback, 
		TFunction<void(const class BHierarchyNode&)> OnHierarchyNodeRead_Callback,
		TFunction<void(const class BGeometryNode&)> OnGeometryNodeRead_Callback,
		TFunction<void(const class BMetadataNode&)> OnMetadataNodeRead_Callback,
		TFunction<void(int32, const FString&)> OnErrorAction);

private:
	void ReadChunk_Internal(uint8* Chunk, int32 Size);
	void Process_Internal();
	int32 ReadUntilFailure(const TArray<uint8>& Input);
	bool TryReadingNode(uint32& ProcessedBytes, const TArray<uint8>& Input, int32 Offset);

	TFunction<void(uint32)> OnFileSDKVersionRead;

	TFunction<void(const class BHierarchyNode&)> OnHierarchyNodeRead;
	TFunction<void(const class BGeometryNode&)> OnGeometryNodeRead;
	TFunction<void(const class BMetadataNode&)> OnMetadataNodeRead;

	TFunction<void(int32, const FString&)> OnError;

	EBNodeType FileType;

	bool bHeaderRead = false;
	int32 WaitingDataBlockQueue_Header_TotalSize = 0;
	TQueue<TArray<uint8>> WaitingDataBlockQueue_Header;

	TQueue<TArray<uint8>> UnprocessedDataQueue; //Concurrent
	bool SafePeek_UnprocessedDataQueue(TArray<uint8>& Result);
	int32 UnprocessedDataSize = 0; //Concurrent
	FCriticalSection UnprocessedDataMutex;

	bool bInnerStreamReadCompleted = false;
	FEvent* ThreadOperationCompleted;
};