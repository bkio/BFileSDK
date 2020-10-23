/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileReader.h"
#include "BInflateDeflate.h"
#include "BLambdaRunnable.h"
#include <stdexcept>

#define UNCOMPRESSED_READ_CHUNK_SIZE 8192

void BFileReader::ReadFromStream(
	std::istream* InStream, 
	EBFileCompressionState InCompressionState, 
	TFunction<void(uint32)> OnFileSDKVersionRead_Callback,
	TFunction<void(const BHierarchyNode&)> OnHierarchyNodeRead_Callback,
	TFunction<void(const BGeometryNode&)> OnGeometryNodeRead_Callback,
	TFunction<void(const BMetadataNode&)> OnMetadataNodeRead_Callback,
	TFunction<void(int32, const FString&)> OnErrorAction)
{
	OnFileSDKVersionRead = OnFileSDKVersionRead_Callback;

	OnHierarchyNodeRead = OnHierarchyNodeRead_Callback;
	OnGeometryNodeRead = OnGeometryNodeRead_Callback;
	OnMetadataNodeRead = OnMetadataNodeRead_Callback;

	OnError = OnErrorAction;

	ThreadOperationCompleted = FGenericPlatformProcess::GetSynchEventFromPool();
	FBLambdaRunnable::RunLambdaOnDedicatedBackgroundThread([this]()
		{
			Process_Internal();
		});

	if (InCompressionState == EBFileCompressionState::Compressed)
	{
		BInflateDeflate::DecompressBytes(InStream, 
			[this](uint8* Buffer, int32 Size)
			{
				ReadChunk_Internal(Buffer, Size);
			},
			[OnErrorAction](const FString& ErrorMessage)
			{
				OnErrorAction(500, ErrorMessage);
			});
	}
	else
	{
		uint8 In[UNCOMPRESSED_READ_CHUNK_SIZE];

		while (InStream->read((char*)In, UNCOMPRESSED_READ_CHUNK_SIZE).gcount() > 0)
		{
			ReadChunk_Internal(In, InStream->gcount());
		}
	}

	bInnerStreamReadCompleted = true;
	ThreadOperationCompleted->Wait();
	FGenericPlatformProcess::ReturnSynchEventToPool(ThreadOperationCompleted);
}

void BFileReader::ReadChunk_Internal(uint8* Chunk, int32 Size)
{
	if (!bHeaderRead)
	{
		if ((WaitingDataBlockQueue_Header_TotalSize + Size) >= X_HEADER_SIZE)
		{
			TArray<uint8> CurrentBlock;
			CurrentBlock.SetNumUninitialized(WaitingDataBlockQueue_Header_TotalSize > 0 ? (WaitingDataBlockQueue_Header_TotalSize + Size) : Size);

			TArray<uint8> WaitingBlock;
			int32 CurrentIx = 0;
			while (WaitingDataBlockQueue_Header.Dequeue(WaitingBlock))
			{
				for (int32 i = 0; i < WaitingBlock.Num(); i++)
					CurrentBlock[CurrentIx++] = WaitingBlock[i];

				WaitingDataBlockQueue_Header_TotalSize -= WaitingBlock.Num();
			}
			for (int32 i = 0; i < Size; i++)
				CurrentBlock[CurrentIx++] = Chunk[i];

			ReadHeader(CurrentBlock.GetData());
			OnFileSDKVersionRead(FileSDKVersion);

			if (CurrentBlock.Num() > X_HEADER_SIZE)
			{
				int32 Count = CurrentBlock.Num() - X_HEADER_SIZE;
				
				TArray<uint8> Rest;
				Rest.SetNumUninitialized(Count);
				for (int32 i = 0; i < Count; i++)
					Rest[i] = CurrentBlock[X_HEADER_SIZE + i];

				{
					FScopeLock ScopeLock(&UnprocessedDataMutex);
					UnprocessedDataQueue.Enqueue(Rest);
					UnprocessedDataSize += Rest.Num();
				}
			}

			bHeaderRead = true;
		}
		else
		{
			WaitingDataBlockQueue_Header.Enqueue(TArray<uint8>(Chunk, Size));
			WaitingDataBlockQueue_Header_TotalSize += Size;
		}
	}
	else
	{
		FScopeLock ScopeLock(&UnprocessedDataMutex);
		UnprocessedDataQueue.Enqueue(TArray<uint8>(Chunk, Size));
		UnprocessedDataSize += Size;
	}
}

bool BFileReader::SafePeek_UnprocessedDataQueue(TArray<uint8>& Result)
{
	FScopeLock ScopeLock(&UnprocessedDataMutex);
	return UnprocessedDataQueue.Peek(Result);
}

void BFileReader::Process_Internal()
{
	TArray<uint8> FailedLeftOverBlock;

	while (true)
	{
		int ProcessedBufferCount = 0;

		int32 UnprocessedDataSize_Current = UnprocessedDataSize;
		if (UnprocessedDataSize_Current > 0)
		{
			TArray<uint8> CurrentBuffer;
			CurrentBuffer.SetNumUninitialized(FailedLeftOverBlock.Num() + UnprocessedDataSize_Current);
			
			for (int32 i = 0; i < FailedLeftOverBlock.Num(); i++)
				CurrentBuffer[i] = FailedLeftOverBlock[i];

			int CurrentIndex = FailedLeftOverBlock.Num();
			FailedLeftOverBlock.Empty();

			TArray<uint8> CurrentBlock;
			while (SafePeek_UnprocessedDataQueue(CurrentBlock) && CurrentIndex < CurrentBuffer.Num())
			{
				for (int32 i = 0; i < CurrentBlock.Num(); i++)
					CurrentBuffer[i + CurrentIndex] = CurrentBlock[i];

				CurrentIndex += CurrentBlock.Num();
				{
					FScopeLock ScopeLock(&UnprocessedDataMutex);

					UnprocessedDataSize -= CurrentBlock.Num();

					TArray<uint8> Ignore;
					UnprocessedDataQueue.Dequeue(Ignore);
				}
				ProcessedBufferCount++;
			}

			int32 SuccessOffset = ReadUntilFailure(CurrentBuffer);
			if (SuccessOffset == -1) continue;

			int32 NewSize = CurrentBuffer.Num() - SuccessOffset;
			FailedLeftOverBlock.SetNumUninitialized(NewSize);

			for (int32 i = 0; i < NewSize; i++)
				FailedLeftOverBlock[i] = CurrentBuffer[i + SuccessOffset];
		}
		if (bInnerStreamReadCompleted && UnprocessedDataQueue.IsEmpty() && UnprocessedDataSize == 0)
		{
			ThreadOperationCompleted->Trigger();
			return;
		}
		if (ProcessedBufferCount < 32)
		{
			FPlatformProcess::Sleep(0.05f);
		}
	}
}
//Returns -1 on full success on reading
int32 BFileReader::ReadUntilFailure(const TArray<uint8>& Input)
{
	uint32 SuccessOffset = 0;

	while (SuccessOffset < (uint32)Input.Num())
	{
		uint32 ProcessedBytes;
		if (!TryReadingNode(ProcessedBytes, Input, SuccessOffset))
		{
			return SuccessOffset;
		}
		SuccessOffset += ProcessedBytes;
	}
	return -1;
}
bool BFileReader::TryReadingNode(uint32& ProcessedBytes, const TArray<uint8>& Input, int32 Offset)
{
	if (Offset >= Input.Num()) return false;

	if (FileType == EBNodeType::EBNodeType_Hierarchy)
	{
		BHierarchyNode NewNode;

		if (!NewNode.FromBytes(ProcessedBytes, Input.GetData() + Offset, Input)) return false;
		
		OnHierarchyNodeRead(NewNode);
	}
	else if (FileType == EBNodeType::EBNodeType_Geometry)
	{
		BGeometryNode NewNode;

		if (!NewNode.FromBytes(ProcessedBytes, Input.GetData() + Offset, Input)) return false;
		
		OnGeometryNodeRead(NewNode);
	}
	else if (FileType == EBNodeType::EBNodeType_Metadata)
	{
		BMetadataNode NewNode;

		if (!NewNode.FromBytes(ProcessedBytes, Input.GetData() + Offset, Input)) return false;
		
		OnMetadataNodeRead(NewNode);
	}
	else
	{
		return false;
	}
	return true;
}